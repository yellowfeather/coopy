/*
** Copyright (c) 2007 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public
** License version 2 as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
** 
** You should have received a copy of the GNU General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*******************************************************************************
**
** This module implements a 3-way merge
*/
#include "config.h"
#include "merge3.h"

#if 0
#define DEBUG(X)  X
#define ISDEBUG 1
#else
#define DEBUG(X)
#define ISDEBUG 0
#endif

/* The minimum of two integers */
#define min(A,B)  (A<B?A:B)

/*
** Compare N lines of text from pV1 and pV2.  If the lines
** are the same, return true.  Return false if one or more of the N
** lines are different.
**
** The cursors on both pV1 and pV2 is unchanged by this comparison.
*/
static int sameLines(Blob *pV1, Blob *pV2, int N){
  char *z1, *z2;
  int i;

  if( N==0 ) return 1;
  z1 = &blob_buffer(pV1)[blob_tell(pV1)];
  z2 = &blob_buffer(pV2)[blob_tell(pV2)];
  for(i=0; z1[i]==z2[i]; i++){
    if( z1[i]=='\n' ){
      N--;
      if( N==0 ) return 1;
    }
  }
  return 0;
}

/*
** Look at the next edit triple in both aC1 and aC2.  (An "edit triple" is
** three integers describing the number of copies, deletes, and inserts in
** moving from the original to the edited copy of the file.) If the three
** integers of the edit triples describe an identical edit, then return 1.  
** If the edits are different, return 0.
*/
static int sameEdit(
  int *aC1,      /* Array of edit integers for file 1 */
  int *aC2,      /* Array of edit integers for file 2 */
  Blob *pV1,     /* Text of file 1 */
  Blob *pV2      /* Text of file 2 */
){
  if( aC1[0]!=aC2[0] ) return 0;
  if( aC1[1]!=aC2[1] ) return 0;
  if( aC1[2]!=aC2[2] ) return 0;
  if( sameLines(pV1, pV2, aC1[2]) ) return 1;
  return 0;
}

/*
** The aC[] array contains triples of integers.  Within each triple, the
** elements are:
**
**   (0)  The number of lines to copy
**   (1)  The number of lines to delete
**   (2)  The number of liens to insert
**
** Suppose we want to advance over sz lines of the originl file.  This routine
** returns true if that advance would land us on a copy operation.  It
** returns false if the advance would end on a delete.
*/
static int ends_at_CPY(int *aC, int sz){
  while( sz>0 && (aC[0]>0 || aC[1]>0 || aC[2]>0) ){
    if( aC[0]>=sz ) return 1;
    sz -= aC[0];
    if( aC[1]>sz ) return 0;
    sz -= aC[1];
    aC += 3;
  }
  return 1;
}

/*
** pSrc contains an edited file where aC[] describes the edit.  Part of
** pSrc has already been output.  This routine outputs additional lines
** of pSrc - lines that correspond to the next sz lines of the original
** unedited file.
**
** Note that sz counts the number of lines of text in the original file.
** But text is output from the edited file.  So the number of lines transfer
** to pOut might be different from sz.  Fewer lines appear in pOut if there
** are deletes.  More lines appear if there are inserts.
**
** The aC[] array is updated and the new index into aC[] is returned.
*/
static int output_one_side(
  Blob *pOut,     /* Write to this blob */
  Blob *pSrc,     /* The edited file that is to be copied to pOut */
  int *aC,        /* Array of integer triples describing the edit */
  int i,          /* Index in aC[] of current location in pSrc */
  int sz          /* Number of lines in unedited source to output */
){
  while( sz>0 ){
    if( aC[i]==0 && aC[i+1]==0 && aC[i+2]==0 ) break;
    if( aC[i]>=sz ){
      blob_copy_lines(pOut, pSrc, sz);
      aC[i] -= sz;
      break;
    }
    blob_copy_lines(pOut, pSrc, aC[i]);
    blob_copy_lines(pOut, pSrc, aC[i+2]);
    sz -= aC[i] + aC[i+1];
    i += 3;
  }
  return i;
}



/*
** Do a three-way merge.  Initialize pOut to contain the result.
**
** The merge is an edit against pV2.  Both pV1 and pV2 have a
** common origin at pPivot.  Apply the changes of pPivot ==> pV1
** to pV2.
**
** The return is 0 upon complete success. If any input file is binary,
** -1 is returned and pOut is unmodified.  If there are merge
** conflicts, the merge proceeds as best as it can and the number 
** of conflicts is returns
*/
int blob_merge(Blob *pPivot, Blob *pV1, Blob *pV2, Blob *pOut){
  int *aC1;              /* Changes from pPivot to pV1 */
  int *aC2;              /* Changes from pPivot to pV2 */
  int i1, i2;            /* Index into aC1[] and aC2[] */
  int nCpy, nDel, nIns;  /* Number of lines to copy, delete, or insert */
  int limit1, limit2;    /* Sizes of aC1[] and aC2[] */
  int nConflict = 0;     /* Number of merge conflicts seen so far */
  static const char zBegin[] = ">>>>>>> BEGIN MERGE CONFLICT\n";
  static const char zMid[]   = "============================\n";
  static const char zEnd[]   = "<<<<<<< END MERGE CONFLICT\n";

  blob_zero(pOut);         /* Merge results stored in pOut */

  /* Compute the edits that occur from pPivot => pV1 (into aC1)
  ** and pPivot => pV2 (into aC2).  Each of the aC1 and aC2 arrays is
  ** an array of integer triples.  Within each triple, the first integer
  ** is the number of lines of text to copy directly from the pivot,
  ** the second integer is the number of lines of text to omit from the
  ** pivot, and the third integer is the number of lines of text that are
  ** inserted.  The edit array ends with a triple of 0,0,0.
  */
  aC1 = text_diff(pPivot, pV1, 0, 0);
  aC2 = text_diff(pPivot, pV2, 0, 0);
  if( aC1==0 || aC2==0 ){
    free(aC1);
    free(aC2);
    return -1;
  }

  blob_rewind(pV1);        /* Rewind inputs:  Needed to reconstruct output */
  blob_rewind(pV2);
  blob_rewind(pPivot);

  /* Determine the length of the aC1[] and aC2[] change vectors */
  for(i1=0; aC1[i1] || aC1[i1+1] || aC1[i1+2]; i1+=3){}
  limit1 = i1;
  for(i2=0; aC2[i2] || aC2[i2+1] || aC2[i2+2]; i2+=3){}
  limit2 = i2;

  DEBUG(
    for(i1=0; i1<limit1; i1+=3){
      printf("c1: %4d %4d %4d\n", aC1[i1], aC1[i1+1], aC1[i1+2]);
    }
    for(i2=0; i2<limit2; i2+=3){
     printf("c2: %4d %4d %4d\n", aC2[i2], aC2[i2+1], aC2[i2+2]);
    }
  )

  /* Loop over the two edit vectors and use them to compute merged text
  ** which is written into pOut.  i1 and i2 are multiples of 3 which are
  ** indices into aC1[] and aC2[] to the edit triple currently being
  ** processed
  */
  i1 = i2 = 0;
  while( i1<limit1 && i2<limit2 ){
    DEBUG( printf("%d: %2d %2d %2d   %d: %2d %2d %2d\n",
           i1/3, aC1[i1], aC1[i1+1], aC1[i1+2],
           i2/3, aC2[i2], aC2[i2+1], aC2[i2+2]); )

    if( aC1[i1]>0 && aC2[i2]>0 ){
      /* Output text that is unchanged in both V1 and V2 */
      nCpy = min(aC1[i1], aC2[i2]);
      DEBUG( printf("COPY %d\n", nCpy); )
      blob_copy_lines(pOut, pPivot, nCpy);
      blob_copy_lines(0, pV1, nCpy);
      blob_copy_lines(0, pV2, nCpy);
      aC1[i1] -= nCpy;
      aC2[i2] -= nCpy;
    }else
    if( aC1[i1] >= aC2[i2+1] && aC1[i1]>0 && aC2[i2+1]+aC2[i2+2]>0 ){
      /* Output edits to V2 that occurs within unchanged regions of V1 */
      nDel = aC2[i2+1];
      nIns = aC2[i2+2];
      DEBUG( printf("EDIT -%d+%d left\n", nDel, nIns); )
      blob_copy_lines(0, pPivot, nDel);
      blob_copy_lines(0, pV1, nDel);
      blob_copy_lines(pOut, pV2, nIns);
      aC1[i1] -= nDel;
      i2 += 3;
    }else
    if( aC2[i2] >= aC1[i1+1] && aC2[i2]>0 && aC1[i1+1]+aC1[i1+2]>0 ){
      /* Output edits to V1 that occur within unchanged regions of V2 */
      nDel = aC1[i1+1];
      nIns = aC1[i1+2];
      DEBUG( printf("EDIT -%d+%d right\n", nDel, nIns); )
      blob_copy_lines(0, pPivot, nDel);
      blob_copy_lines(0, pV2, nDel);
      blob_copy_lines(pOut, pV1, nIns);
      aC2[i2] -= nDel;
      i1 += 3;
    }else
    if( sameEdit(&aC1[i1], &aC2[i2], pV1, pV2) ){
      /* Output edits that are identical in both V1 and V2. */
      assert( aC1[i1]==0 );
      nDel = aC1[i1+1];
      nIns = aC1[i1+2];
      DEBUG( printf("EDIT -%d+%d both\n", nDel, nIns); )
      blob_copy_lines(0, pPivot, nDel);
      blob_copy_lines(pOut, pV1, nIns);
      blob_copy_lines(0, pV2, nIns);
      i1 += 3;
      i2 += 3;
    }else
    {
      /* We have found a region where different edits to V1 and V2 overlap.
      ** This is a merge conflict.  Find the size of the conflict, then
      ** output both possible edits separate by distinctive marks.
      */
      int sz = 1;    /* Size of the conflict in lines */
      nConflict++;
      while( !ends_at_CPY(&aC1[i1], sz) || !ends_at_CPY(&aC2[i2], sz) ){
        sz++;
      }
      DEBUG( printf("CONFLICT %d\n", sz); )
      blob_appendf(pOut, zBegin);
      i1 = output_one_side(pOut, pV1, aC1, i1, sz);
      blob_appendf(pOut, zMid);
      i2 = output_one_side(pOut, pV2, aC2, i2, sz);
      blob_appendf(pOut, zEnd);
      blob_copy_lines(0, pPivot, sz);
    }

    /* If we are finished with an edit triple, advance to the next
    ** triple.
    */
    if( i1<limit1 && aC1[i1]==0 && aC1[i1+1]==0 && aC1[i1+2]==0 ) i1+=3;
    if( i2<limit2 && aC2[i2]==0 && aC2[i2+1]==0 && aC2[i2+2]==0 ) i2+=3;
  }

  /* When one of the two edit vectors reaches its end, there might still
  ** be an insert in the other edit vector.  Output this remaining
  ** insert.
  */
  DEBUG( printf("%d: %2d %2d %2d   %d: %2d %2d %2d\n",
         i1/3, aC1[i1], aC1[i1+1], aC1[i1+2],
         i2/3, aC2[i2], aC2[i2+1], aC2[i2+2]); )
  if( i1<limit1 && aC1[i1+2]>0 ){
    DEBUG( printf("INSERT +%d left\n", aC1[i1+2]); )
    blob_copy_lines(pOut, pV1, aC1[i1+2]);
  }else if( i2<limit2 && aC2[i2+2]>0 ){
    DEBUG( printf("INSERT +%d right\n", aC2[i2+2]); )
    blob_copy_lines(pOut, pV2, aC2[i2+2]);
  }

  free(aC1);
  free(aC2);
  return nConflict;
}

/*
** COMMAND:  test-3-way-merge
**
** Combine change in going from PIVOT->VERSION1 with the change going
** from PIVOT->VERSION2 and write the combined changes into MERGED.
*/
void delta_3waymerge_cmd(void){
  Blob pivot, v1, v2, merged;
  if( g.argc!=6 ){
    fprintf(stderr,"Usage: %s %s PIVOT V1 V2 MERGED\n", g.argv[0], g.argv[1]);
    exit(1);
  }
  if( blob_read_from_file(&pivot, g.argv[2])<0 ){
    fprintf(stderr,"cannot read %s\n", g.argv[2]);
    exit(1);
  }
  if( blob_read_from_file(&v1, g.argv[3])<0 ){
    fprintf(stderr,"cannot read %s\n", g.argv[3]);
    exit(1);
  }
  if( blob_read_from_file(&v2, g.argv[4])<0 ){
    fprintf(stderr,"cannot read %s\n", g.argv[4]);
    exit(1);
  }
  blob_merge(&pivot, &v1, &v2, &merged);
  if( blob_write_to_file(&merged, g.argv[5])<blob_size(&merged) ){
    fprintf(stderr,"cannot write %s\n", g.argv[4]);
    exit(1);
  }
  blob_reset(&pivot);
  blob_reset(&v1);
  blob_reset(&v2);
  blob_reset(&merged);
}