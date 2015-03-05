/*** dround.c -- perform simple date arithmetic, round to duration
 *
 * Copyright (C) 2012-2015 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of dateutils.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "dt-core.h"
#include "dt-io.h"
#include "dt-core-tz-glue.h"
#include "prchunk.h"
/* parsers and formatters */
#include "date-core-strpf.h"

#if !defined assert
# define assert(x)
#endif	/* !assert */

const char *prog = "dround";


static bool
durs_only_d_p(struct dt_dt_s dur[], size_t ndur)
{
	for (size_t i = 0; i < ndur; i++) {
		if (dur[i].t.typ) {
			return false;
		}
	}
	return true;
}

static struct dt_t_s
tround_tdur(struct dt_t_s t, struct dt_t_s dur, bool nextp)
{
/* this will return the rounded to DUR time of T and, to encode carry
 * (which can only take values 0 or 1), we will use t's neg bit */
	signed int mdur;
	signed int diff;

	/* no dur is a no-op */
	if (UNLIKELY(!dur.sdur)) {
		return t;
	}

	/* unpack t */
	mdur = t.hms.h * SECS_PER_HOUR +
		t.hms.m * SECS_PER_MIN +
		t.hms.s;
	if (!(diff = mdur % dur.sdur) && !t.hms.ns && !nextp) {
		return t;
	}

	/* compute the result */
	mdur -= diff;
	if (dur.sdur > 0 || nextp) {
		mdur += dur.sdur;
	}
	if (UNLIKELY(mdur < 0)) {
		/* lift */
		mdur += SECS_PER_DAY;
		/* denote the carry */
		t.neg = 1;
	} else if (UNLIKELY(mdur >= (signed)SECS_PER_DAY)) {
		t.neg = 1;
	}
	/* and convert back */
	t.hms.ns = 0;
	t.hms.s = mdur % SECS_PER_MIN;
	mdur /= SECS_PER_MIN;
	t.hms.m = mdur % MINS_PER_HOUR;
	mdur /= MINS_PER_HOUR;
	t.hms.h = mdur % HOURS_PER_DAY;
	return t;
}

static struct dt_d_s
dround_ddur(struct dt_d_s d, struct dt_d_s dur, bool nextp)
{
	switch (dur.typ) {
		unsigned int tgt;
		bool forw;
	case DT_DAISY:
		if (dur.daisydur > 0) {
			tgt = dur.daisydur;
			forw = true;
		} else if (dur.daisydur < 0) {
			tgt = -dur.daisydur;
			forw = false;
		} else {
			/* user is an idiot */
			break;
		}

		switch (d.typ) {
			unsigned int mdays;
		case DT_YMD:
			if ((forw && d.ymd.d < tgt) ||
			    (!forw && d.ymd.d > tgt)) {
				/* no month or year adjustment */
				;
			} else if (d.ymd.d == tgt && !nextp) {
				/* we're ON the date already and no
				 * next/prev date is requested */
				;
			} else if (forw) {
				if (LIKELY(d.ymd.m < GREG_MONTHS_P_YEAR)) {
					d.ymd.m++;
				} else {
					d.ymd.m = 1;
					d.ymd.y++;
				}
			} else {
				if (UNLIKELY(--d.ymd.m < 1)) {
					d.ymd.m = GREG_MONTHS_P_YEAR;
					d.ymd.y--;
				}
			}
			/* get ultimo */
			mdays = __get_mdays(d.ymd.y, d.ymd.m);
			if (UNLIKELY(tgt > mdays)) {
				tgt = mdays;
			}
			/* final assignment */
			d.ymd.d = tgt;
			break;
		default:
			break;
		}
		break;

	case DT_BIZSI:
		/* bizsis only work on bizsidurs atm */
		if (dur.bizsidur > 0) {
			tgt = dur.bizsidur;
			forw = true;
		} else if (dur.bizsidur < 0) {
			tgt = -dur.bizsidur;
			forw = false;
		} else {
			/* user is an idiot */
			break;
		}

		switch (d.typ) {
			unsigned int bdays;
		case DT_BIZDA:
			if ((forw && d.bizda.bd < tgt) ||
			    (!forw && d.bizda.bd > tgt)) {
				/* no month or year adjustment */
				;
			} else if (d.bizda.bd == tgt && !nextp) {
				/* we're ON the date already and no
				 * next/prev date is requested */
				;
			} else if (forw) {
				if (LIKELY(d.bizda.m < GREG_MONTHS_P_YEAR)) {
					d.bizda.m++;
				} else {
					d.bizda.m = 1;
					d.bizda.y++;
				}
			} else {
				if (UNLIKELY(--d.bizda.m < 1)) {
					d.bizda.m = GREG_MONTHS_P_YEAR;
					d.bizda.y--;
				}
			}
			/* get ultimo */
			bdays = __get_bdays(d.bizda.y, d.bizda.m);
			if (UNLIKELY(tgt > bdays)) {
				tgt = bdays;
			}
			/* final assignment */
			d.bizda.bd = tgt;
			break;
		default:
			break;
		}
		break;

	case DT_YMD:
		switch (d.typ) {
			unsigned int mdays;
		case DT_YMD:
			forw = !dur.neg;
			tgt = dur.ymd.m;

			if ((forw && d.ymd.m < tgt) ||
			    (!forw && d.ymd.m > tgt)) {
				/* no year adjustment */
				;
			} else if (d.ymd.m == tgt && !nextp) {
				/* we're IN the month already and no
				 * next/prev date is requested */
				;
			} else if (forw) {
				/* years don't wrap around */
				d.ymd.y++;
			} else {
				/* years don't wrap around */
				d.ymd.y--;
			}
			/* final assignment */
			d.ymd.m = tgt;
			/* fixup ultimo mismatches */
			mdays = __get_mdays(d.ymd.y, d.ymd.m);
			if (UNLIKELY(d.ymd.d > mdays)) {
				d.ymd.d = mdays;
			}
			break;
		default:
			break;
		}
		break;

	case DT_YMCW: {
		struct dt_d_s tmp;
		unsigned int wday;
		signed int diff;

		forw = !dur.neg;
		tgt = dur.ymcw.w;

		tmp = dt_dconv(DT_DAISY, d);
		wday = dt_get_wday(tmp);
		diff = (signed)tgt - (signed)wday;


		if ((forw && wday < tgt) ||
		    (!forw && wday > tgt)) {
			/* nothing to do */
			;
		} else if (wday == tgt && !nextp) {
			/* we're on WDAY already, do fuckall */
			;
		} else if (forw) {
			/* week wrap */
			diff += 7;
		} else {
			/* week wrap */
			diff -= 7;
		}

		/* final assignment */
		tmp.daisy += diff;
		d = dt_dconv(d.typ, tmp);
		break;
	}

	case DT_MD:
	default:
		break;
	}
	return d;
}

static struct dt_d_s one_day = {
	.typ = DT_DAISY,
	.dur = 1,
	.neg = 0,
};

static struct dt_dt_s
dt_round(struct dt_dt_s d, struct dt_dt_s dur, bool nextp)
{
	if (dt_sandwich_only_t_p(dur) && d.sandwich) {
		d.t = tround_tdur(d.t, dur.t, nextp);
		/* check carry */
		if (UNLIKELY(d.t.neg == 1)) {
			/* we need to add a day */
			one_day.daisydur = 1 | -(dur.t.sdur < 0);
			d.t.neg = 0;
			d.d = dt_dadd(d.d, one_day);
		}
	} else if (dt_sandwich_only_d_p(dur) &&
		   (dt_sandwich_p(d) || dt_sandwich_only_d_p(d))) {
		unsigned int sw = d.sandwich;
		d.d = dround_ddur(d.d, dur.d, nextp);
		d.sandwich = (uint8_t)sw;
	}
	return d;
}


static struct dt_dt_s
dround(struct dt_dt_s d, struct dt_dt_s dur[], size_t ndur, bool nextp)
{
	for (size_t i = 0; i < ndur; i++) {
		d = dt_round(d, dur[i], nextp);
	}
	return d;
}

/* extended duration reader */
static int
dt_io_strpdtrnd(struct __strpdtdur_st_s *st, const char *str)
{
	char *sp = NULL;
	struct strpd_s d = strpd_initialiser();
	struct dt_spec_s s = spec_initialiser();
	struct dt_dt_s payload = dt_dt_initialiser();
	bool negp = false;

	if (dt_io_strpdtdur(st, str) >= 0) {
		return 0;
	}

	/* check if there's a sign + or - */
	if (*str == '-') {
		negp = true;
		str++;
	} else if (*str == '+') {
		str++;
	}

	/* try weekdays, set up s */
	s.spfl = DT_SPFL_S_WDAY;
	s.abbr = DT_SPMOD_NORM;
	if (__strpd_card(&d, str, s, &sp) >= 0) {
		payload.d = dt_make_ymcw(0, 0, 0, d.w);
		/* make sure it's d-only */
		payload.sandwich = 0;
		goto out;
	}

	/* try months, set up s */
	s.spfl = DT_SPFL_S_MON;
	s.abbr = DT_SPMOD_NORM;
	if (__strpd_card(&d, str, s, &sp) >= 0) {
		payload.d = dt_make_ymd(0, d.m, 0);
		/* make sure it's d-only */
		payload.sandwich = 0;
		goto out;
	}

	/* bugger */
	st->istr = str;
	return -1;
out:
	st->sign = 0;
	st->cont = NULL;
	payload.neg = negp;
	return __add_dur(st, payload);
}

struct prln_ctx_s {
	struct grep_atom_soa_s *ndl;
	const char *ofmt;
	zif_t fromz;
	zif_t outz;
	zif_t hackz;
	int sed_mode_p;
	int quietp;

	const struct __strpdtdur_st_s *st;
	bool nextp;
};

static int
proc_line(struct prln_ctx_s ctx, char *line, size_t llen)
{
	struct dt_dt_s d;
	char *sp = NULL;
	char *ep = NULL;
	int rc = 0;

	do {
		/* check if line matches, */
		d = dt_io_find_strpdt2(line, ctx.ndl, &sp, &ep, ctx.fromz);

		if (!dt_unk_p(d)) {
			if (UNLIKELY(d.fix) && !ctx.quietp) {
				rc = 2;
			}
			/* perform addition now */
			d = dround(d, ctx.st->durs, ctx.st->ndurs, ctx.nextp);

			if (ctx.hackz == NULL && ctx.fromz != NULL) {
				/* fixup zone */
				d = dtz_forgetz(d, ctx.fromz);
			}

			if (ctx.sed_mode_p) {
				__io_write(line, sp - line, stdout);
				dt_io_write(d, ctx.ofmt, ctx.outz, '\0');
				llen -= (ep - line);
				line = ep;
			} else {
				dt_io_write(d, ctx.ofmt, ctx.outz, '\n');
				break;
			}
		} else if (ctx.sed_mode_p) {
			line[llen] = '\n';
			__io_write(line, llen + 1, stdout);
			break;
		} else {
			/* obviously unmatched, warn about it in non -q mode */
			if (!ctx.quietp) {
				dt_io_warn_strpdt(line);
				rc = 2;
			}
			break;
		}
	} while (1);
	return rc;
}


#include "dround.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct dt_dt_s d;
	struct __strpdtdur_st_s st = __strpdtdur_st_initialiser();
	char *inp;
	const char *ofmt;
	char **fmt;
	size_t nfmt;
	int rc = 0;
	bool dt_given_p = false;
	bool nextp = false;
	zif_t fromz = NULL;
	zif_t z = NULL;
	zif_t hackz = NULL;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (argi->nargs == 0U) {
		error("Error: DATE or DURATION must be specified\n");
		yuck_auto_help(argi);
		rc = 1;
		goto out;
	}
	/* init and unescape sequences, maybe */
	ofmt = argi->format_arg;
	fmt = argi->input_format_args;
	nfmt = argi->input_format_nargs;
	if (argi->backslash_escapes_flag) {
		dt_io_unescape(argi->format_arg);
		for (size_t i = 0; i < nfmt; i++) {
			dt_io_unescape(fmt[i]);
		}
	}

	/* try and read the from and to time zones */
	if (argi->from_zone_arg) {
		fromz = dt_io_zone(argi->from_zone_arg);
	}
	if (argi->zone_arg) {
		z = dt_io_zone(argi->zone_arg);
	}
	if (argi->next_flag) {
		nextp = true;
	}

	/* check first arg, if it's a date the rest of the arguments are
	 * durations, if not, dates must be read from stdin */
	for (size_t i = 0U; i < argi->nargs; i++) {
		inp = argi->args[i];
		do {
			if (dt_io_strpdtrnd(&st, inp) < 0) {
				if (UNLIKELY(i == 0)) {
					/* that's ok, must be a date then */
					dt_given_p = true;
				} else {
					serror("Error: \
cannot parse duration/rounding string `%s'", st.istr);
				}
			}
		} while (__strpdtdur_more_p(&st));
	}
	/* check if there's only d durations */
	hackz = durs_only_d_p(st.durs, st.ndurs) ? NULL : fromz;

	/* sanity checks */
	if (dt_given_p) {
		/* date parsing needed postponing as we need to find out
		 * about the durations */
		inp = argi->args[0U];
		if (dt_unk_p(d = dt_io_strpdt(inp, fmt, nfmt, hackz))) {
			error("Error: \
cannot interpret date/time string `%s'", argi->args[0U]);
			rc = 1;
			goto out;
		}
	} else if (st.ndurs == 0) {
		error("Error: \
no durations given");
		rc = 1;
		goto out;
	}

	/* start the actual work */
	if (dt_given_p) {
		if (UNLIKELY(d.fix) && !argi->quiet_flag) {
			rc = 2;
		}
		if (!dt_unk_p(d = dround(d, st.durs, st.ndurs, nextp))) {
			if (hackz == NULL && fromz != NULL) {
				/* fixup zone */
				d = dtz_forgetz(d, fromz);
			}
			dt_io_write(d, ofmt, z, '\n');
		} else {
			rc = 1;
		}
	} else {
		/* read from stdin */
		size_t lno = 0;
		struct grep_atom_s __nstk[16], *needle = __nstk;
		size_t nneedle = countof(__nstk);
		struct grep_atom_soa_s ndlsoa;
		void *pctx;
		struct prln_ctx_s prln = {
			.ndl = &ndlsoa,
			.ofmt = ofmt,
			.fromz = fromz,
			.outz = z,
			.hackz = hackz,
			.sed_mode_p = argi->sed_mode_flag,
			.quietp = argi->quiet_flag,
			.st = &st,
			.nextp = nextp,
		};

		/* no threads reading this stream */
		__io_setlocking_bycaller(stdout);

		/* lest we overflow the stack */
		if (nfmt >= nneedle) {
			/* round to the nearest 8-multiple */
			nneedle = (nfmt | 7) + 1;
			needle = calloc(nneedle, sizeof(*needle));
		}
		/* and now build the needle */
		ndlsoa = build_needle(needle, nneedle, fmt, nfmt);


		/* using the prchunk reader now */
		if ((pctx = init_prchunk(STDIN_FILENO)) == NULL) {
			serror("Error: could not open stdin");
			goto ndl_free;
		}
		while (prchunk_fill(pctx) >= 0) {
			for (char *line; prchunk_haslinep(pctx); lno++) {
				size_t llen = prchunk_getline(pctx, &line);

				rc |= proc_line(prln, line, llen);
			}
		}
		/* get rid of resources */
		free_prchunk(pctx);
	ndl_free:
		if (needle != __nstk) {
			free(needle);
		}
		goto out;
	}
	/* free the strpdur status */
	__strpdtdur_free(&st);

	dt_io_clear_zones();

out:
	yuck_free(argi);
	return rc;
}

/* dround.c ends here */
