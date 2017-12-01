#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define MAX_FILE_DATA_SEG_BYTES ((8*1024*1024)/512 - (1 + 10 + 10*10))*512

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                                   /* File size in bytes. */
    off_t EOF_pos;                                  /* Allocation frontier */
    block_sector_t start;                           /* First data sector. */
    block_sector_t direct_block_array[10];          /* Direct blocks */
    block_sector_t single_indirect_block_array[10]; /* Indirect block */
    block_sector_t double_indirect_block_array[10]; /* Doubly indirect blocks */
    unsigned magic;                                 /* Magic number. */
    uint32_t unused[
       125-((30*sizeof(block_sector_t)+sizeof(off_t))/(sizeof(uint32_t)))
            ];                                      /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;               /* Element in inode list. */
    block_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                        /* Number of openers. */
    bool removed;                        /* True if deleted, false otherwise. */
    int deny_write_cnt;                  /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;            /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos, bool extend) 
{
  ASSERT (inode != NULL);
  /*
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
  */

  /* Helper variables */
  uint8_t *bounce = malloc (BLOCK_SECTOR_SIZE);
  block_sector_t target;

  /* Validate pos */
  if (pos < MAX_FILE_DATA_SEG_BYTES)
    {
    /* Fetch location of data specified by metadata */
    if (pos < inode->data.EOF_pos)
      {
        int sector_num = pos / BLOCK_SECTOR_SIZE;
        if (pos < inode->data.length)
          {
            /* Read from direct inode portion */
            if (sector_num < 10)
              return inode->data.direct_block_array[sector_num];
            /* Read from single indirect portion */
            else if (sector_num < (10 + 10*10))
              {
                /* Read the single indirect block into bounce */
                int index_within_inode_section = (sector_num - 10)/10;
                block_read (fs_device, 
                  inode->data.single_indirect_block_array[
                    index_within_inode_section], bounce);
                /* Read location specified by single indirect slot into target */
                int index_within_single_indirect_section = (sector_num - 10) % 10;
                memcpy (&target, bounce + index_within_single_indirect_section*
                  sizeof (block_sector_t), sizeof (block_sector_t));
                free (bounce);
                return target;
              }
            /* Read from double indirect portion */
            else if (sector_num < (10 + 10*10 + 10*10*10))
              {
                /* Read the double indirect block into bounce */
                int index_within_inode_section = (sector_num - 100 - 10)/100;
                block_read (fs_device, 
                  inode->data.single_indirect_block_array[
                    index_within_inode_section], bounce);
                /* Read location specified by double indirect slot into target */
                int index_within_double_indirect_section = 
                    (sector_num - 100 - 10) / 10;
                memcpy (&target, bounce + index_within_double_indirect_section*
                  sizeof (block_sector_t), sizeof (block_sector_t));
                /* Read the single indirect block into bounce */
                block_read (fs_device, target, bounce);
                /* Read location specified by single indirect slot into target */
                int index_within_single_indirect_section = 
                    (sector_num - 100 - 10) % 10;
                memcpy (&target, bounce + index_within_single_indirect_section*
                  sizeof (block_sector_t), sizeof (block_sector_t));
                free (bounce);
                return target;
              }
            }
        /* Allocate new data sector, record in metadata */
        else if (extend)
          {
            block_sector_t direct_block_location;
            /* Allocate in direct portion */
            if (sector_num < 10)
              {
                /* Allocate a sector to store the data */
                if (!free_map_allocate (1, &direct_block_location))
                  return -1;
                block_write (fs_device, inode->data.direct_block_array[
                  sector_num], &direct_block_location);
                memcpy((inode->data.single_indirect_block_array +
                      sector_num), &direct_block_location, 
                      sizeof(block_sector_t));
                return direct_block_location;
              }
            sector_num -= 10;
            block_sector_t single_indirect_block_location;
            /* Allocate in single indirect portion */
            if (sector_num < (10*10))
              {
                /* Allocate another single indirect block if necessary,
                   and get the sector acting as that block */
                if (sector_num % 10 == 0)
                  {
                    if (!free_map_allocate (1, &single_indirect_block_location))
                      return -1;
                    memcpy((inode->data.single_indirect_block_array +
                      (sector_num/10)), &single_indirect_block_location, 
                      sizeof(block_sector_t));
                  }
                single_indirect_block_location = 
                  inode->data.single_indirect_block_array[sector_num/10];
                /* Allocate a sector to store the data */
                if (!free_map_allocate (1, &direct_block_location))
                  return -1;
                block_write (fs_device, single_indirect_block_location +
                  sizeof(block_sector_t)*(sector_num%10), 
                  &direct_block_location);
              }
            sector_num -= 100;
            block_sector_t double_indirect_block_location;
            bool double_added = false;
            /* Allocate in double indirect portion */
            if (sector_num < (10*10*10))
              {
                /* Allocate another double indirect block if necessary,
                   and get the sector acting as the double indirect block */
                if (sector_num % 100 == 0)
                  {
                    if (!free_map_allocate (1, 
                      &double_indirect_block_location))
                      return -1;
                    memcpy((inode->data.double_indirect_block_array +
                      (sector_num/100)), &double_indirect_block_location, 
                      sizeof(block_sector_t));
                    double_added = true;
                  }
                double_indirect_block_location = 
                  inode->data.double_indirect_block_array[sector_num/100];
                /* Allocate another single indirect block if necessary,
                   and get the sector acting as that indirect block */
                if ((sector_num % 10) == 0 || double_added)
                  {
                    if (!free_map_allocate(1,
                      &single_indirect_block_location))
                      return -1;
                    block_write (fs_device, double_indirect_block_location
                      + ((sector_num - 100)/10)*sizeof(block_sector_t), 
                      &single_indirect_block_location);
                  }
                else
                  {
                    block_read (fs_device, double_indirect_block_location, 
                      bounce);
                    memcpy (&single_indirect_block_location, 
                      (bounce + ((sector_num - 100)/10)*sizeof(block_sector_t)), 
                      sizeof (block_sector_t));
                  }
                /* Allocate a sector to store the data */
                if (!free_map_allocate (1, &direct_block_location))
                  return -1;
                block_write (fs_device, single_indirect_block_location
                  + sizeof(block_sector_t)*(sector_num%10),
                  &direct_block_location);
              }
          }
      }
    }

  free (bounce);
  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  //bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
     /*
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
      */

      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->EOF_pos = length;

      /* How to divide blocks */
      size_t total_sectors = bytes_to_sectors(length);
      int direct_blocks_needed = (total_sectors > 10) ? 10 : total_sectors;
      total_sectors -= 10;
      int single_indirect_blocks_needed = (total_sectors > 100) ? 100 : 
          total_sectors;
      total_sectors -= 100;
      int double_indirect_blocks_needed = total_sectors;

      /* Helper variables */
      static char zeros[BLOCK_SECTOR_SIZE];
      static block_sector_t pointers_from_single_indirect_block[10];
      static block_sector_t pointers_from_double_indirect_block[10];
      int block_num, single_indirect_block_index, double_indirect_block_index;
      int i;

      /* Allocate necessary direct portion sectors */
      for (block_num = 0; block_num < direct_blocks_needed; block_num++)
        {
          /* Point current inode slot to a sector acting 
               as a data block */
          if (!free_map_allocate (1, 
            &disk_inode->direct_block_array[block_num]))
            return false;
          /* Zero out the data block pointed to by this slot */
          block_write (fs_device, disk_inode->direct_block_array[block_num], 
            zeros); 
        }

      /* Allocate necessary single indirect portion sectors */
      for (block_num = 0; block_num < single_indirect_blocks_needed; 
           block_num += 10)
        {
          int blocks_needed_current_single_indirect = 
            ((single_indirect_blocks_needed - block_num) >= 10) ? 10 : 
            (single_indirect_blocks_needed - block_num);
          /* Point current inode slot to a sector acting 
               as a single indirect block */
          if (!free_map_allocate (1, 
            &disk_inode->single_indirect_block_array[block_num/10]))
            return false;
          /* Allocate a single data block for each single indirect slot */
          for (single_indirect_block_index = 0; single_indirect_block_index < 
               blocks_needed_current_single_indirect; 
               single_indirect_block_index++)
           {
             /* Point current single indirect block slot to a data block */
             if (!free_map_allocate (1, &pointers_from_single_indirect_block[
                  single_indirect_block_index]))
               return false;
             /* Zero out the data block pointed to by this indirect slot */
             block_write (fs_device, pointers_from_single_indirect_block[
              single_indirect_block_index], zeros);
           }
           /* Write out the single indirect block's pointers to 
                 their appropriate slots in the indirect block sector */
           block_write (fs_device, disk_inode->single_indirect_block_array[
             block_num/10], pointers_from_single_indirect_block);
        }
      for (i = 0; i < 10; i++) 
        pointers_from_single_indirect_block[i] ^= 0;

      /* Allocate necessary double indirect portion sectors */
      for (block_num = 0; block_num < double_indirect_blocks_needed; 
           block_num += 100)
        {
          int blocks_needed_current_double_indirect = 
            ((double_indirect_blocks_needed - block_num) >= 100) ? 100 :
            (double_indirect_blocks_needed - block_num);
          /* Point current inode slot to a sector acting
             as a double indirect block */
          if (!free_map_allocate (1, 
            &disk_inode->double_indirect_block_array[block_num/100]))
            return false;
          /* Allocate a single indirect block for each double indirect slot */
          for (double_indirect_block_index = 0; 
            double_indirect_block_index < blocks_needed_current_double_indirect;
            double_indirect_block_index += 10)
            {
              int blocks_needed_current_single_indirect = 
              ((double_indirect_blocks_needed - double_indirect_block_index - 
                block_num) >= 10) ? 10 : (double_indirect_blocks_needed - 
              double_indirect_block_index - block_num); 
              /* Point currnet double indirect block slot to a sector acting as
                 a single direct block */
              if (!free_map_allocate (1, &pointers_from_double_indirect_block[
                double_indirect_block_index/10]))
                return false;
              /* Allocate a single data block for each single indirect slot */
              for (single_indirect_block_index = 0; single_indirect_block_index 
                < blocks_needed_current_single_indirect;
                   single_indirect_block_index++)
                {
                  /* Point current single indirect block slot to a data block */
                  if (!free_map_allocate (1, 
                    &pointers_from_single_indirect_block[
                      single_indirect_block_index]))
                    return false;
                  /* Zero out the data block pointed to by this indirect slot */
                  block_write (fs_device, 
                    pointers_from_single_indirect_block[
                      single_indirect_block_index], zeros);
                }
              /* Write out the single indirect block's pointers to 
                 their appropriate slots in the indirect block sector */
              block_write (fs_device, pointers_from_double_indirect_block[
                double_indirect_block_index/10], 
                pointers_from_single_indirect_block);
            }
          /* Write out the double indirect block's pointers to 
                 their appropriate slots in the double block sector */
          block_write (fs_device, disk_inode->double_indirect_block_array[
            block_num/100], pointers_from_double_indirect_block);
        }

      block_write (fs_device, sector, disk_inode);

      free (disk_inode);
    }

  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {

      //  Disk sector to read, starting byte offset within sector.
      block_sector_t sector_idx = byte_to_sector (inode, offset, false);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      // Bytes left in inode, bytes left in sector, lesser of the two. 
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      // Number of bytes to actually copy out of this sector. 
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          // Read full sector directly into caller's buffer.
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          // Read sector into bounce buffer, then partially copy
          //   into caller's buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      // Advance. 
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;

    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  off_t endpoint = offset + size;
  off_t gap = 0;
  bool gap_added = false;
  if ((endpoint > inode->data.length) && (endpoint < MAX_FILE_DATA_SEG_BYTES))
    inode->data.EOF_pos += (endpoint - inode->data.length);
  if (offset > inode->data.length)
    gap = offset - inode->data.length;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, true);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode to result, bytes left in sector, lesser of the two. */
      off_t inode_left = endpoint - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
      if (((off_t) (sector_idx*BLOCK_SECTOR_SIZE)) > inode->data.length)
        {
          if (!gap_added)
            inode->data.length += gap;
          inode->data.length += BLOCK_SECTOR_SIZE;
        }
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
