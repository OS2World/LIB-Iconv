/* OS/2 iconv() implementation through OS/2 Unicode API
   Copyright (C) 2001-2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

/*
   This file implements an iconv wrapper based on OS/2 Unicode API.
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

# include <uconv.h>


typedef struct _iconv_t
{
  UconvObject from;		/* "From" conversion handle */
  UconvObject to;		/* "To" conversion handle */
} *iconv_t;

/* Tell "iconv.h" to not define iconv_t by itself.  */
#define _ICONV_T
#include "my_iconv.h"

#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <alloca.h>


struct cp_replacement {
  const char *alias;
  const char *os2name;
};


/* list of codepage aliases that are not recognized by the OS/2
   Unicode API. */
static struct cp_replacement aliases[] = {
  { "EUC-JP",      "IBM-954"  },
  { "EUC-TW",      "IBM-964"  },
  { "EUC-KR",      "IBM-970"  },
  { "BIG5",        "IBM-950"  },
  { "EUC-CN",      "IBM-1383" },
  { "ISO-8859-15", "IBM-923"  },
  { NULL,          NULL }         /* end of list */
};
    

/* greatest length of an IBM-xxxx charset name without trailing 0 */
#define __MAX_ALIAS_LEN__ 8


/* Convert an encoding name to te form understood by UniCreateUconvObject.  */
static void
cp_convert (const char *cp, UniChar *ucp)
{
  /* note: UniChar is a wchar_t which is of type unsigned short */
  size_t i = 0;

  /* check whether cp is just an alias */
  while (aliases[i].alias != NULL)
    {
      if (stricmp(cp, aliases[i].alias) == 0)
	{
	  /* we found an alias, replace it by the "real" name and
	     leave the while loop */
	  cp = aliases[i].os2name;
	  break;
	}
      i++;
    }

  /* Transform CPXXX naming style to IBM-XXX style */
  if ((cp[0] == 'C' || cp[0] == 'c') && (cp[1] == 'P' || cp[1] == 'p'))
    {
      cp += 2; /* remove the 2 first characters */
      ucp[0] = 'I';
      ucp[1] = 'B';
      ucp[2] = 'M';
      ucp[3] = '-';
      ucp += 4; /* remove the 4 first characters */

      /* copy the remaining bytes */
      while (*cp != '\0')
	*ucp++ = *cp++;

      /* trailing 0 */
      *ucp = '\0';
    }

  else
    {
      while (*cp != '\0')
	*ucp++ = *cp++;

      /* trailing 0 */
      *ucp = '\0';
    }
}


iconv_t
iconv_open (const char *cp_to, const char *cp_from)
{
  UniChar *ucp;
  iconv_t conv;
  uconv_attribute_t attr;
  size_t ucp_size;

  conv = (iconv_t) malloc (sizeof (struct _iconv_t));
  if (conv == NULL)
    {
      errno = ENOMEM;
      return (iconv_t)(-1);
    }

  ucp_size = strlen (cp_from);
  if (ucp_size < __MAX_ALIAS_LEN__) ucp_size = __MAX_ALIAS_LEN__;
  ucp_size = (ucp_size + 1) * sizeof(UniChar); /* including trailing 0 */
  ucp = (UniChar *) alloca (ucp_size);

  cp_convert (cp_from, ucp);
  if (UniCreateUconvObject (ucp, &conv->from))
    {
      free (conv);
      errno = EINVAL;
      return (iconv_t)(-1);
    }

  ucp_size = strlen (cp_to);
  if (ucp_size < __MAX_ALIAS_LEN__) ucp_size = __MAX_ALIAS_LEN__;
  ucp_size = (ucp_size + 1) * sizeof(UniChar); /* including trailing 0 */
  ucp = (UniChar *) alloca (ucp_size);

  cp_convert (cp_to, ucp);
  if (UniCreateUconvObject (ucp, &conv->to))
    {
      UniFreeUconvObject (conv->from);
      free (conv);
      errno = EINVAL;
      return (iconv_t)(-1);
    }

  UniQueryUconvObject (conv->from, &attr, sizeof (attr), NULL, NULL, NULL);
  /* Do not treat 0x7f as a control character
     (don't understand what it exactly means but without it MBCS prefix
     character detection sometimes could fail (when 0x7f is a prefix)).
     And don't treat the string as a path (the docs also don't explain
     what it exactly means, but I'm pretty sure converted texts will
     mostly not be paths).  */
  attr.converttype &= ~(CVTTYPE_CTRL7F | CVTTYPE_PATH);
  UniSetUconvObject (conv->from, &attr);

  return conv;
}

/*
A different case is when inbuf is NULL or *inbuf is NULL, but outbuf is not NULL and *outbuf is
not NULL. In this case, the iconv function attempts to set cd's conversion state to the initial state
and store a corresponding shift sequence at *outbuf. At most *outbytesleft bytes, starting at
*outbuf, will be written. If the output buffer has no more room for this reset sequence, it sets
errno to E2BIG and returns (size_t)(-1). Otherwise it increments *outbuf and decrements
*outbytesptr by the number of bytes written.

A third case is when inbuf is NULL or *inbuf is NULL, and outbuf is NULL or *outbuf is NULL.
In this case, the iconv function sets cd's conversion state to the initial state.
*/

static size_t
iconv7 (iconv_t conv,
       const char **in, size_t *in_left,
       char **out, size_t *out_left, int slack, int *overflow)
{
  int rc1 = 0, rc2 = 0;
  size_t tmp_left, non_id1, ucs_converted, non_id2 = 0;
  UniChar *ucs;
  UniChar *orig_ucs;
  const char *in_ini;
  size_t in_left_ini;

  if (in == NULL || *in == NULL)
    {
      /* This is a special case: According to the GNU iconv docs "conv"
	 has to be reset, whatever this means. There is also a distiction
	 whether (out == NULL || *out == NULL) or not. I assume a nop
	 here. */
      *overflow = 0;
      return 0;
    }

  in_ini = *in;
  in_left_ini = *in_left;
  if (in_ini == 0) {	/* boundary case */
    *overflow = 0;
    return 0;
  }

  tmp_left = *in_left;
  if (tmp_left > *out_left)
    tmp_left = *out_left;
  tmp_left += slack;	      /* Might be a few unicode chars per input char */
  if (tmp_left > 16*1024)
    tmp_left = 16*1024;		/* Do not overload alloca() */
  ucs = (UniChar *) alloca (tmp_left * sizeof (UniChar));
  orig_ucs = ucs;

  rc1 = UniUconvToUcs (conv->from, (void **)in, in_left, &ucs, &tmp_left, &non_id1);
  if (ucs == orig_ucs)		/* Do not need to do anything else */
    goto end_work;

  ucs_converted = ucs - orig_ucs;
  ucs = orig_ucs;
  /* UniUconvFromUcs will stop at first NULL byte
     while we want ALL the bytes converted.  */
#if 1
  rc2 = UniUconvFromUcs (conv->to, &ucs, &ucs_converted, (void **)out, out_left, &non_id2);
  if (ucs_converted == 0)  /* If all intermediate buffer is read, all done; */
    goto end_work;

  /* Not all converted; calculate the converted amount
     in terms of the original encoding */
  *in = in_ini;			/* Updates done by UniUconvToUcs() stale now */
  *in_left = in_left_ini;
  if (ucs == orig_ucs)		/* No conversion done */
    goto end_work;
  ucs_converted = ucs - orig_ucs;
  ucs = orig_ucs;
  rc1 = UniUconvToUcs (conv->from, (void **)in, in_left, &ucs, &ucs_converted, &non_id1);
  /* There should not be an error condition now!  rc1 must be 0. */
#else
  while (ucs_converted)
    {
      size_t usl = 0;
      while (ucs_converted && (ucs[usl] != 0))
        usl++, ucs_converted--;
      rc = UniUconvFromUcs (conv->to, &ucs, &usl, (void **)out, out_left, &non_id2);
      if (rc)
        goto error;
      non_id1 += non_id2;
      if (ucs_converted && *out_left)
        {
          *(*out)++ = 0;
          (*out_left)--;
          ucs++; ucs_converted--;
        }
    }
#endif

 end_work:
  /* Convert two OS/2 error codes to errno.  If only one is present,
     then it determines what happened.  */
  if (!rc2 && rc1 == ULS_BUFFERFULL) {	/* intermediate overflow */
    *overflow = 1;
    return non_id1 + non_id2;		/* XXXX Can we do better??? */
  }
  *overflow = 0;			/* There was no overflow */
  switch (rc2 ? rc2 : rc1)  /* Error on step 1 is erased if the step 2 did not succeed to the end */
  {
    case 0:
      return non_id1 + non_id2;
    case ULS_ILLEGALSEQUENCE:
      errno = EILSEQ;
      break;
    case ULS_INVALID:
      errno = EINVAL;
      break;
    case ULS_BUFFERFULL:	/* Automatically on step 2 */
      errno = E2BIG;
      break;
    default:
      errno = EBADF;
      break;
  }
  return (size_t)(-1);
}


size_t
iconv (iconv_t conv,
       const char **in, size_t *in_left,
       char **out, size_t *out_left)
{
  int overflow;
  const char *in_ini = (in ? *in : 0);
  size_t in_left_ini = (in_left ? *in_left : 0), rc, slack = 0;

 redo_conv:
  rc = iconv7(conv, in, in_left, out, out_left, slack, &overflow);
  if (overflow) {
    if (in_ini == *in)		/* Too little slack, nothing done */
      slack = slack * 2 + 10;
    in_ini = *in;
    goto redo_conv;
  }
  return rc;
}

int
iconv_close (iconv_t conv)
{
  if (conv != (iconv_t)(-1))
    {
      UniFreeUconvObject (conv->to);
      UniFreeUconvObject (conv->from);
      free (conv);
    }
  return 0;
}
