/* Copyright (c) 2009-2024 AO MCST.
   Copyright (C) 1991-2024 Free Software Foundation, Inc.
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, see
   <https://www.gnu.org/licenses/>.  */

#include "sysdep.h"
#include "bfd.h"
#include "elf-bfd.h"
#include "elfxx-e2k.h"
#include "elfxx-e2k-pm.h"

static bool
segment_executable (struct elf_segment_map *seg)
{
  if (seg->p_flags_valid)
    return (seg->p_flags & PF_X) != 0;
  else
    {
      /* The p_flags value has not been computed yet,
	 so we have to look through the sections.  */
      unsigned int i;
      for (i = 0; i < seg->count; ++i)
	if (seg->sections[i]->flags & SEC_CODE)
	  return true;
    }
  return false;
}

/* Determine if this segment is eligible to receive the file and program
   headers.  It must be read-only and non-executable.
   Its first section must start far enough past the page boundary to
   allow space for the headers.  */
static bool
segment_eligible_for_headers (struct elf_segment_map *seg,
			      bfd_vma minpagesize, bfd_vma sizeof_headers)
{
  unsigned int i;
  if (seg->count == 0 || seg->sections[0]->lma % minpagesize < sizeof_headers)
    return false;
  for (i = 0; i < seg->count; ++i)
    {
      if ((seg->sections[i]->flags & (SEC_CODE|SEC_READONLY)) != SEC_READONLY)
	return false;
    }
  return true;
}


bool
_bfd_e2k_pm_elf_modify_segment_map (bfd *abfd, struct bfd_link_info *info)
{
  const struct elf_backend_data *const bed = get_elf_backend_data (abfd);
  struct elf_segment_map **m = &elf_seg_map (abfd);
  struct elf_segment_map **first_load = NULL;
  struct elf_segment_map **last_load = NULL;
  bool moved_headers = false;
  int sizeof_headers;

  if (info != NULL)
    /* We're doing linking, so evalute SIZEOF_HEADERS as in a linker script.  */
    sizeof_headers = bfd_sizeof_headers (abfd, info);
  else
    {
      /* We're not doing linking, so this is objcopy or suchlike.
	 We just need to collect the size of the existing headers.  */
      struct elf_segment_map *seg;
      sizeof_headers = bed->s->sizeof_ehdr;
      for (seg = *m; seg != NULL; seg = seg->next)
	sizeof_headers += bed->s->sizeof_phdr;
    }

  while (*m != NULL)
    {
      struct elf_segment_map *seg = *m;

      if (seg->p_type == PT_LOAD)
	{
	  bool executable = segment_executable (seg);

	  /* First, we're just finding the earliest PT_LOAD.
	     By the normal rules, this will be the lowest-addressed one.
	     We only have anything interesting to do if it's executable.  */
	  last_load = m;
	  if (first_load == NULL)
	    {
	      if (!executable)
		goto next;
	      first_load = m;
	    }
	  /* Now that we've noted the first PT_LOAD, we're looking for
	     the first non-executable PT_LOAD with a nonempty p_filesz.  */
	  else if (!moved_headers
		   && segment_eligible_for_headers (seg, bed->minpagesize,
						    sizeof_headers))
	    {
	      /* This is the one we were looking for!

		 First, clear the flags on previous segments that
		 say they include the file header and phdrs.  */
	      struct elf_segment_map *prevseg;
	      for (prevseg = *first_load;
		   prevseg != seg;
		   prevseg = prevseg->next)
		if (prevseg->p_type == PT_LOAD)
		  {
		    prevseg->includes_filehdr = 0;
		    prevseg->includes_phdrs = 0;
		  }

	      /* This segment will include those headers instead.  */
	      seg->includes_filehdr = 1;
	      seg->includes_phdrs = 1;

	      moved_headers = true;
	    }
	}

    next:
      m = &seg->next;
    }

  if (first_load != last_load && moved_headers)
    {
      /* Now swap the first and last PT_LOAD segments'
	 positions in segment_map.  */
      struct elf_segment_map *first = *first_load;
      struct elf_segment_map *last = *last_load;
      *first_load = first->next;
      first->next = last->next;
      last->next = first;
    }

  return true;
}

bool
_bfd_e2k_pm_elf_object_p (bfd *abfd)
{
  return _bfd_e2k_elf_object_p_1 (abfd, true);
}
