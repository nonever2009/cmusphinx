/* -*- c-basic-offset: 4 -*- */
/* ====================================================================
 * Copyright (c) 1996-2000 Carnegie Mellon University.  All rights 
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
/*********************************************************************
 *
 * File: s3gau_full_io.c
 * 
 * Description: 
 *     Gaussian full covariance matrix file I/O
 * Author: 
 *     Eric Thayer (eht@cs.cmu.edu)
 *     David Huggins-Daines (dhuggins@cs.cmu.edu)
 *********************************************************************/

#include <s3/s3gau_io.h>
#include <s3/matrix.h>
#include <s3/s3io.h>

#include <s3/ckd_alloc.h>
#include <s3/s3.h>

#include <assert.h>
#include <string.h>

int
s3gau_read_maybe_full(const char *fn,
		      vector_t *****out,
		      uint32 *out_n_mgau,
		      uint32 *out_n_feat,
	  	      uint32 *out_n_density,
		      const uint32 **out_veclen,
		      uint32 need_full)
{
    FILE *fp;
    const char *do_chk;
    const char *ver;
    uint32 n_mgau, n_feat, n_density;
    uint32 *veclen, maxveclen;
    uint32 blk, i, j, k, l, r, n;
    uint32 chksum = 0;
    uint32 sv_chksum, ignore = 0;
    float32 *raw;
    vector_t ****o;
    uint32 swap;

    fp = s3open(fn, "rb", &swap);
    if (fp == NULL)
	return S3_ERROR;

    /* check version id */
    ver = s3get_gvn_fattr("version");
    if (ver) {
	if (strcmp(ver, GAU_FILE_VERSION) != 0) {
	    E_FATAL("Version mismatch for %s, file ver: %s != reader ver: %s\n",
		    fn, ver, GAU_FILE_VERSION);
	}
    }
    else {
	E_FATAL("No version attribute for %s\n", fn);
    }
    
    /* if do_chk is non-NULL, there is a checksum after the data in the file */
    do_chk = s3get_gvn_fattr("chksum0");

    if (do_chk && !strcmp(do_chk, "no")) {
        do_chk = NULL;
    }
    
    if (s3read(&n_mgau, sizeof(uint32), 1, fp, swap, &chksum) != 1) {
	goto error;
    }

    if (s3read(&n_feat, sizeof(uint32), 1, fp, swap, &chksum) != 1) {
	goto error;
    }

    if (s3read(&n_density, sizeof(uint32), 1, fp, swap, &chksum) != 1) {
	goto error;
    }

    veclen = ckd_calloc(n_feat, sizeof(uint32));
    if (s3read(veclen, sizeof(uint32), n_feat, fp, swap, &chksum) != n_feat) {
	goto error;
    }

    if (s3read_1d((void **)&raw, sizeof(float32), &n, fp, swap, &chksum) != S3_SUCCESS) {
	ckd_free(veclen);

	goto error;
    }

    for (i = 0, blk = 0, maxveclen = 0; i < n_feat; i++) {
	blk += veclen[i] * veclen[i];
	if (veclen[i] > maxveclen) maxveclen = veclen[i];
    }
    if (n != n_mgau * n_density * blk) {
	if (need_full)
	     E_ERROR("Failed to read full covariance file %s (expected %d values, got %d)\n",
	     	     fn, n_mgau * n_density * blk, n);
	goto error;
    }

    o = (vector_t ****)ckd_calloc_4d(n_mgau, n_feat, n_density,
				     maxveclen, sizeof(vector_t));

    for (i = 0, r = 0; i < n_mgau; i++) {
	for (j = 0; j < n_feat; j++) {
	    for (k = 0; k < n_density; k++) {
		for (l = 0; l < veclen[j]; l++) {
		    o[i][j][k][l] = &raw[r];

		    r += veclen[j];
		}
	    }
	}
    }

    if (do_chk) {
	/* See if the checksum in the file matches that which
	   was computed from the read data */

	if (s3read(&sv_chksum, sizeof(uint32), 1, fp, swap, &ignore) != 1) {
            goto error;
	}

	if (sv_chksum != chksum) {
	    E_FATAL("Checksum error; read corrupt data.\n");
	}
    }


    *out = o;
    *out_n_mgau = n_mgau;
    *out_n_feat = n_feat;
    *out_n_density = n_density;
    *out_veclen = veclen;

    s3close(fp);

    E_INFO("Read %s [%ux%ux%u array of full matrices]\n",
	   fn, n_mgau, n_feat, n_density);

    return S3_SUCCESS;

error:
    if (fp) s3close(fp);

    return S3_ERROR;
}

int
s3gau_read_full(const char *fn,
		vector_t *****out,
		uint32 *out_n_mgau,
		uint32 *out_n_feat,
		uint32 *out_n_density,
		const uint32 **out_veclen)
{
    return s3gau_read_maybe_full(fn, out, out_n_mgau, out_n_feat, 
                                 out_n_density, out_veclen, TRUE);
}

int
s3gau_write_full(const char *fn,
		 const vector_t ****out,
		 uint32 n_mgau,
		 uint32 n_feat,
		 uint32 n_density,
		 const uint32 *veclen)
{
    FILE *fp = NULL;
    uint32 chksum = 0;
    uint32 blk, i, ignore = 0;

    s3clr_fattr();
    s3add_fattr("version", GAU_FILE_VERSION, TRUE);
    s3add_fattr("chksum0", "yes", TRUE);

    fp = s3open(fn, "wb", NULL);
    if (fp == NULL)
	return S3_ERROR;

    if (s3write(&n_mgau, sizeof(uint32), 1, fp, &chksum) != 1) {
	goto error;
    }

    if (s3write(&n_feat, sizeof(uint32), 1, fp, &chksum) != 1) {
	goto error;
    }

    if (s3write(&n_density, sizeof(uint32), 1, fp, &chksum) != 1) {
	goto error;
    }

    if (s3write(veclen, sizeof(uint32), n_feat, fp, &chksum) != n_feat) {
	goto error;
    }

    for (blk = 0, i = 0; i < n_feat; i++)
	blk += veclen[i] * veclen[i];

    if (s3write_1d(out[0][0][0][0], sizeof(float32),
		   n_mgau*n_density*blk, fp, &chksum) != S3_SUCCESS) {
	goto error;
    }

    if (s3write(&chksum, sizeof(uint32), 1, fp, &ignore) != 1) {
	goto error;
    }

    s3close(fp);

    E_INFO("Wrote %s [%ux%ux%u array of full matrices]\n",
	   fn, n_mgau, n_feat, n_density);

    return S3_SUCCESS;

error:
    if (fp) s3close(fp);
    return S3_ERROR;
}

int
s3gaucnt_read_full(const char *fn,
		   vector_t ****out_wt_mean,
		   vector_t *****out_wt_var,
		   int32 *out_pass2var,
		   float32 ****out_dnom,
		   uint32 *out_n_cb,
		   uint32 *out_n_feat,
		   uint32 *out_n_density,
		   const uint32 **out_veclen)
{
    uint32 rd_chksum = 0;
    uint32 sv_chksum;
    uint32 ignore;
    char *ver;
    char *do_chk;
    FILE *fp;
    uint32 swap;

    uint32 has_means;
    uint32 has_vars;
    uint32 pass2var;
    uint32 n_cb;
    uint32 n_feat;
    uint32 n_density;
    uint32 *veclen;
    float32 *buf;
    float32 ***dnom;
    uint32 n, i, b_i, j, k, l, d1, d2, d3;
    vector_t ***wt_mean = NULL;
    vector_t ****wt_var = NULL;

    fp = s3open(fn, "rb", &swap);
    if (fp == NULL)
	return S3_ERROR;

    /* check version id */
    ver = s3get_gvn_fattr("version");
    if (ver) {
	if (strcmp(ver, GAUCNT_FILE_VERSION) != 0) {
	    E_FATAL("Version mismatch for %s, file ver: %s != reader ver: %s\n",
		    fn, ver, GAUCNT_FILE_VERSION);
	}
    }
    else {
	E_FATAL("No version attribute for %s\n", fn);
    }
    
    /* if do_chk is non-NULL, there is a checksum after the data in the file */
    do_chk = s3get_gvn_fattr("chksum0");

    if (s3read((void *)&has_means, sizeof(uint32), 1, fp, swap, &rd_chksum) != 1) {
	return S3_ERROR;
    }

    if (s3read((void *)&has_vars, sizeof(uint32), 1, fp, swap, &rd_chksum) != 1) {
	return S3_ERROR;
    }

    if (s3read((void *)&pass2var, sizeof(uint32), 1, fp, swap, &rd_chksum) != 1) {
	return S3_ERROR;
    }

    if (s3read((void *)&n_cb, sizeof(uint32), 1, fp, swap, &rd_chksum) != 1) {
	return S3_ERROR;
    }

    if (s3read((void *)&n_density, sizeof(uint32), 1, fp, swap, &rd_chksum) != 1) {
	return S3_ERROR;
    }

    if (s3read_1d((void **)&veclen, sizeof(uint32), &n_feat, fp, swap, &rd_chksum) != S3_SUCCESS) {
	return S3_ERROR;
    }

    if (has_means) {
	if (s3read_1d((void *)&buf, sizeof(float32), &n, fp, swap, &rd_chksum) != S3_SUCCESS) {
	    return S3_ERROR;
	}
	
	wt_mean = (vector_t ***)ckd_calloc_3d(n_cb, n_feat, n_density, sizeof(vector_t));

	for (i = 0, b_i = 0; i < n_cb; i++) {
	    for (j = 0; j < n_feat; j++) {
		for (k = 0; k < n_density; k++) {
		    wt_mean[i][j][k] = &buf[b_i];

		    b_i += veclen[j];
		}
	    }
	}
    }

    if (has_vars) {
	uint32 blk, maxveclen;

	for (i = 0, blk = 0, maxveclen = 0; i < n_feat; i++) {
	    blk += veclen[i];
	    if (veclen[i] > maxveclen) maxveclen = veclen[i];
	}

	if (s3read_1d((void *)&buf, sizeof(float32), &n, fp, swap, &rd_chksum) != S3_SUCCESS) {
	    return S3_ERROR;
	}
	assert(n == n_cb * n_density * blk * blk);
	
	wt_var = (vector_t ****)ckd_calloc_4d(n_cb, n_feat, n_density,
					      maxveclen, sizeof(vector_t));

	for (i = 0, b_i = 0; i < n_cb; i++) {
	    for (j = 0; j < n_feat; j++) {
		for (k = 0; k < n_density; k++) {
		    for (l = 0; l < veclen[j]; l++) {
			wt_var[i][j][k][l] = &buf[b_i];

			b_i += veclen[j];
		    }
		}
	    }
	}
    }

    if (s3read_3d((void ****)&dnom, sizeof(float32), &d1, &d2, &d3, fp, swap, &rd_chksum) != S3_SUCCESS) {
	return S3_ERROR;
    }

    assert(d1 == n_cb);
    assert(d2 == n_feat);
    assert(d3 == n_density);

    if (do_chk) {
	/* See if the checksum in the file matches that which
	   was computed from the read data */
	
	if (s3read(&sv_chksum, sizeof(uint32), 1, fp, swap, &ignore) != 1) {
	    s3close(fp);
	    return S3_ERROR;
	}
	
	if (sv_chksum != rd_chksum) {
	    E_FATAL("Checksum error; read corrupt data.\n");
	}
    }
    
    s3close(fp);

    *out_wt_mean = wt_mean;
    *out_wt_var = wt_var;
    *out_pass2var = pass2var;
    *out_dnom = dnom;
    *out_n_cb = n_cb;
    *out_n_feat = n_feat;
    *out_n_density = n_density;
    *out_veclen = veclen;

    E_INFO("Read %s%s%s%s [%ux%ux%u vector arrays]\n",
	   fn,
	   (has_means ? " with means" : ""),
	   (has_vars ? " with vars" : ""),
	   (has_vars && pass2var ? " (2pass)" : ""),
	   n_cb, n_feat, n_density);

    return S3_SUCCESS;
}

int
s3gaucnt_write_full(const char *fn,
		    vector_t ***wt_mean,
		    vector_t ****wt_var,
		    int32 pass2var,
		    float32 ***dnom,
		    uint32 n_cb,
		    uint32 n_feat,
		    uint32 n_density,
		    const uint32 *veclen)
{
    FILE *fp;
    uint32 chksum = 0;
    uint32 ignore = 0;
    uint32 n_elem, blk, j, has_means, has_vars;

    s3clr_fattr();
    s3add_fattr("version", GAUCNT_FILE_VERSION, TRUE);
    s3add_fattr("chksum0", "yes", TRUE);

    fp = s3open(fn, "wb", NULL);
    if (fp == NULL)
	return S3_ERROR;


    if (wt_mean != NULL)
	has_means = TRUE;
    else
	has_means = FALSE;
    if (s3write((void *)&has_means, sizeof(uint32), 1, fp, &chksum) != 1) {
	return S3_ERROR;
    }

    if (wt_var != NULL)
	has_vars = TRUE;
    else
	has_vars = FALSE;
    if (s3write((void *)&has_vars, sizeof(uint32), 1, fp, &chksum) != 1) {
	return S3_ERROR;
    }
    if (s3write((void *)&pass2var, sizeof(uint32), 1, fp, &chksum) != 1) {
	return S3_ERROR;
    }
    if (s3write((void *)&n_cb, sizeof(uint32), 1, fp, &chksum) != 1) {
	return S3_ERROR;
    }
    if (s3write((void *)&n_density, sizeof(uint32), 1, fp, &chksum) != 1) {
	return S3_ERROR;
    }

    if (s3write_1d((void *)veclen, sizeof(uint32), n_feat, fp, &chksum) != S3_SUCCESS) {
	return S3_ERROR;
    }
    
    for (j = 0, blk = 0; j < n_feat; j++)
	blk += veclen[j];

    n_elem = n_cb * n_density * blk;

    if (has_means) {
	band_nz_1d(wt_mean[0][0][0], n_elem, MIN_POS_FLOAT32);
	
	if (s3write_1d((void *)wt_mean[0][0][0], sizeof(float32), n_elem, fp, &chksum) != S3_SUCCESS)
	    return S3_ERROR;
    }

    if (has_vars) {
	/* Don't floor full variances!!! */
	if (s3write_1d((void *)wt_var[0][0][0][0], sizeof(float32),
		       n_elem * blk, fp, &chksum) != S3_SUCCESS)
	    return S3_ERROR;
    }

    /* floor all non-zero entries to this value to make sure
       that results are compatible between machines */
    floor_nz_3d(dnom, n_cb, n_feat, n_density, MIN_POS_FLOAT32);

    if (s3write_3d((void ***)dnom, sizeof(float32),
		   n_cb, n_feat, n_density, fp, &chksum) != S3_SUCCESS) {
	return S3_ERROR;
    }

    if (s3write(&chksum, sizeof(uint32), 1, fp, &ignore) != 1) {
	s3close(fp);
	
	return S3_ERROR;
    }
	
    s3close(fp);

    E_INFO("Wrote %s%s%s%s [%ux%ux%u vector/matrix arrays]\n",
	   fn,
	   (has_means ? " with means" : ""),
	   (has_vars ? " with full vars" : ""),
	   (has_vars && pass2var ? " (2pass)" : ""),
	   n_cb, n_feat, n_density);

    return S3_SUCCESS;
}
/*
 * Log record.  Maintained by RCS.
 *
 * $Log$
 * 
 *
 */



