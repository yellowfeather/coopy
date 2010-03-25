/*
** Copyright (c) 2002 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
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
** This program reads C source code from standard input.  Lines that
** begin with the "@" character are translated into cgi_printf() statements
** and the translated code is written on standard output.
**
** The problem this program is attempt to solve is as follows:  When
** writing CGI programs in C, we typically want to output a lot of HTML
** text to standard output.  In pure C code, this involves doing a
** printf() with a big string containing all that text.  But we have
** to insert special codes (ex: \n and \") for many common characters,
** which interferes with the readability of the HTML.
**
** This tool allows us to put raw HTML, without the special codes, in
** the middle of a C program.  This program then translates the text
** into standard C by inserting all necessary backslashes and other
** punctuation.
*/
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/*
** Space to hold arguments at the end of the cgi_printf()
*/
#define MX_ARG_SP 10000
static char zArg[MX_ARG_SP];
static int nArg = 0;

/*
** True if we are currently in a cgi_printf()
*/
static int inPrint = 0;

/*
** True if we are currently doing a free string
*/
static int inStr = 0;

/*
** Terminate an active cgi_printf() or free string
*/
static void end_block(FILE *out){
  if( inPrint ){
    zArg[nArg] = 0;
    fprintf(out, "%s);\n", zArg);
    nArg = 0;
    inPrint = 0;
  }
}

/*
** Translate the input stream into the output stream
*/
static void trans(FILE *in, FILE *out){
  int i, j, k;        /* Loop counters */
  char c1, c2;        /* Characters used to start a comment */
  int lastWasEq = 0;  /* True if last non-whitespace character was "=" */
  char zLine[2000];   /* A single line of input */
  char zOut[4000];    /* The input line translated into appropriate output */

  c1 = c2 = '-';
  while( fgets(zLine, sizeof(zLine), in) ){
    for(i=0; zLine[i] && isspace(zLine[i]); i++){}
    if( zLine[i]!='@' ){
      if( inPrint || inStr ) end_block(out);
      /*
      if (strncmp(zLine+i,"db_finalize",11)==0) {
	zLine[i] = '\0';
	fprintf(out,"%s",zLine);
	fprintf(out,"printf(\"db_finalize %%s:%%d\\n\",__FILE__,__LINE__); d");
	fprintf(out,"%s",zLine+i+1);
	} else if (strncmp(zLine+i,"blob_reset",10)==0) {
	zLine[i] = '\0';
	fprintf(out,"%s",zLine);
	fprintf(out,"printf(\"blob_reset %%s:%%d\\n\",__FILE__,__LINE__); b");
	fprintf(out,"%s",zLine+i+1);
      } else if (strncmp(zLine+i,"db_static",9)==0) {
	zLine[i] = '\0';
	fprintf(out,"%s",zLine);
	fprintf(out,"printf(\"db_static %%s:%%d\\n\",__FILE__,__LINE__); d");
	fprintf(out,"%s",zLine+i+1);
      } else
      */
      if (strncmp(zLine+i,"printf",6)==0) {
	zLine[i] = '\0';
	fprintf(out,"%s",zLine);
	fprintf(out,"_ssfossil_p");
	fprintf(out,"%s",zLine+i+1);
      } else if (strncmp(zLine+i,"exit(",5)==0) {
	zLine[i] = '\0';
	fprintf(out,"%s",zLine);
	fprintf(out,"_ssfossil_e");
	fprintf(out,"%s",zLine+i+1);
      } else if (strncmp(zLine+i,"fprintf(stderr",14)==0) {
	zLine[i] = '\0';
	fprintf(out,"%s",zLine);
	fprintf(out,"_ssfossil_f");
	fprintf(out,"%s",zLine+i+1);
      } else {
	fprintf(out,"%s",zLine);
      }
                       /* 0123456789 12345 */
      if( strncmp(zLine, "/* @-comment: ", 14)==0 ){
        c1 = zLine[14];
        c2 = zLine[15];
      }
      i += strlen(&zLine[i]);
      while( i>0 && isspace(zLine[i-1]) ){ i--; }
      lastWasEq = i>0 && zLine[i-1]=='=';
    }else if( lastWasEq ){
      /* If the last non-whitespace character before the first @ was
      ** an "=" then generate a string literal.  But skip comments
      ** consisting of all text between c1 and c2 (default "--")
      ** and end of line.
      */
      int indent, omitline;
      i++;
      if( isspace(zLine[i]) ){ i++; }
      indent = i - 2;
      if( indent<0 ) indent = 0;
      omitline = 0;
      for(j=0; zLine[i] && zLine[i]!='\r' && zLine[i]!='\n'; i++){
        if( zLine[i]==c1 && (c2==' ' || zLine[i+1]==c2) ){
           omitline = 1; break; 
        }
        if( zLine[i]=='"' || zLine[i]=='\\' ){ zOut[j++] = '\\'; }
        zOut[j++] = zLine[i];
      }
      while( j>0 && isspace(zOut[j-1]) ){ j--; }
      zOut[j] = 0;
      if( j<=0 && omitline ){
        fprintf(out,"\n");
      }else{
        fprintf(out,"%*s\"%s\\n\"\n",indent, "", zOut);
      }
    }else{
      /* Otherwise (if the last non-whitespace was not '=') then generate
      ** a cgi_printf() statement whose format is the text following the '@'.
      ** Substrings of the form "%C(...)" where C is any character will
      ** puts "%C" in the format and add the "..." as an argument to the
      ** cgi_printf call.
      */
      int indent;
      i++;
      if( isspace(zLine[i]) ){ i++; }
      indent = i;
      for(j=0; zLine[i] && zLine[i]!='\r' && zLine[i]!='\n'; i++){
        if( zLine[i]=='"' || zLine[i]=='\\' ){ zOut[j++] = '\\'; }
        zOut[j++] = zLine[i];
        if( zLine[i]!='%' || zLine[i+1]=='%' || zLine[i+1]==0 ) continue;
        if( zLine[i+2]!='(' ) continue;
        i++;
        zOut[j++] = zLine[i];
        zArg[nArg++] = ',';
        i += 2;
        k = 1;
        while( zLine[i] ){
          if( zLine[i]==')' ){
            k--;
            if( k==0 ) break;
          }else if( zLine[i]=='(' ){
            k++;
          }
          zArg[nArg++] = zLine[i++];
        }
      }
      zOut[j] = 0;
      if( !inPrint ){
        fprintf(out,"%*scgi_printf(\"%s\\n\"",indent-2,"", zOut);
        inPrint = 1;
      }else{
        fprintf(out,"\n%*s\"%s\\n\"",indent+5, "", zOut);
      }
    }      
  }
}

int main(int argc, char **argv){
  if( argc==2 ){
    FILE *in = fopen(argv[1], "r");
    if( in==0 ){
      fprintf(stderr,"can not open %s\n", argv[1]);
      exit(1);
    }
    trans(in, stdout);
    fclose(in);
  }else{
    trans(stdin, stdout);
  }
  return 0;
}