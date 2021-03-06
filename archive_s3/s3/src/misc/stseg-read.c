/* ====================================================================
 * Copyright (c) 1995-2002 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*
 * stseg.c -- Read and display .stseg file created by s3align.
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1996 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * 19-Jul-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* "\nCI.8 LC.8 RC.8 POS.3(HI)-ST.5(LO) SCR(32)" */

static char *phone[100];
static int n_phone;
static char *posname = "besiu";


static skip_line (FILE *fp)
{
    int c;
    
    while (((c = fgetc (fp)) >= 0) && (c != '\n'));
}


main ()
{
    int c, i, k, nf, scr;
    FILE *fp;
    char str[1024];
    
    fp = stdin;
    n_phone = 0;
    
    /* Skip version# string */
    skip_line (fp);
    
    /* Read CI phone names */
    for (;;) {
	for (i = 0;; i++) {
	    if (((c = fgetc(fp)) == ' ') || (c == '\n'))
		break;
	    str[i] = c;
	}
	str[i] = '\0';
	
	if (c == ' ') {
	    phone[n_phone] = (char *) malloc (i+1);
	    strcpy (phone[n_phone], str);
	    n_phone++;
	} else
	    break;
    }
    printf ("%d phones\n", n_phone);
    
    /* Skip format line */
    skip_line (fp);
    
    /* Skip end-comment line */
    skip_line (fp);

    /* Read byteorder magic no. */
    fread (&i, sizeof(int), 1, fp);
    assert (i == 0x11223344);
    
    /* Read no. frames */
    fread (&nf, sizeof(int), 1, fp);
    printf ("#frames = %d\n", nf);
    
    /* Read state info per frame */
    for (i = 0; i < nf; i++) {
	k = fread (str, sizeof(char), 4, fp);
	assert (k == 4);
	k = fread (&scr, sizeof(int), 1, fp);
	assert (k == 1);

	c = str[0];
	assert ((c >= 0) && (c < n_phone));
	printf ("%5d %11d %2d %s", i, scr, str[3] & 0x001f, phone[c]);

	c = str[1];
	if (c != -1) {
	    assert ((c >= 0) && (c < n_phone));
	    printf (" %s", phone[c]);
	}

	c = str[2];
	if (c != -1) {
	    assert ((c >= 0) && (c < n_phone));
	    printf (" %s", phone[c]);
	}
	
	c = (str[3] >> 5) & 0x07;
	if ((c >= 0) && (c < 4))
	    printf (" %c", posname[c]);

	printf ("\n");
    }
}
