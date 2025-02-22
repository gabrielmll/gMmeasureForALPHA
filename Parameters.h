// Copyright 2007,2008,2009,2010,2011,2012,2013,2014,2015 Loïc Cerf (magicbanana@gmail.com)

// This file is part of multidupehack.

// multidupehack is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 3 as published by the Free Software Foundation

// multidupehack is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along with multidupehack; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

// Output
/* OUTPUT turns on the output. This option may be disabled to evaluate the performance of multidupehack independently from the writing performances of the disk. */
#define OUTPUT

// Heuristics (defaults should be kept unless the gain brought by a heuristics is evaluated)
/* PRE_PROCESS turns on the pre-process that aims to ignore elements that cannot be in any pattern given the minimal size constraints and the tolerance to noise. */
#define PRE_PROCESS

/* MIN_SIZE_ELEMENT_PRUNING turns on the heuristic that removes from the search space individual elements that would necessarily make a pattern violate some minimal size or area constraint. */
#define MIN_SIZE_ELEMENT_PRUNING

/* DETECT_NON_EXTENSION_ELEMENTS turns out the heuristic that removes the elements that are set absent by enumeration and cannot prevent the closedness of the patterns that will be recursively considered. */
#define DETECT_NON_EXTENSION_ELEMENTS

/* ENUMERATION_PROCESS selects the procedure that decides what is the dimension the next enumerated element is taken in: */
/* 0: choose the dimension first, then choose the element w.r.t the noise it introduces in the potential part of the search space */
/* 1: choose the dimension first, then choose the element w.r.t. the noise it introduces in the present part of the search space (in present and potential in case of equality) */
#define ENUMERATION_PROCESS 0

// Log
/* VERBOSE_PARSER turns on the output (on the standard output) of information when the input data and group files are parsed. */
/* #define VERBOSE_PARSER */

/* DEBUG turns on the output (on the standard output) of information during the extraction of the closed error-tolerant n-sets. This option may be enabled by someone who wishes to precisely understand how this extraction is performed on a small data set. */
/* #define DEBUG */

/* VERBOSE_DIM_CHOICE turns on the output (on the standard output) of information regarding the choice of the next dimension to be enumerated. Combined with DEBUG, this option may be enabled by someone who wishes to precisely understand how this choice is performed on a small data set. */
/* #define VERBOSE_DIM_CHOICE */

/* NUMERIC_PRECISION turns ou the output (on the standard output) of the maximal possible round-off error made when internally storing a membership degree. */
/* #define NUMERIC_PRECISION */

/* NB_OF_CLOSED_N_SETS turns on the output (on the standard output) of how many closed error-tolerant n-sets were selected during the extraction phase. */
/* #define NB_OF_CLOSED_N_SETS */

/* NB_OF_LEFT_NODES turns on the output (on the standard output) of how many patterns were considered during the extraction phase, i.e., the count of valid (but not always maximal) patterns with regard to the applied constraints. This option may be enabled by someone who wishes a measure of the time complexity independent from the hardware performance. */
/* #define NB_OF_LEFT_NODES */

/* TIME turns on the output (on the standard output) of the running time of multidupehack. */
/* #define TIME */

/* DETAILED_TIME turns on the output (on the standard output) of a more detailed analysis of how the time is spent. It gives (in this order): */
/* - the parsing time */
/* - the pre-processing time (to reduce the relation) */
/* - the mining time */
/* - the post-processing time (to agglomerate the closed error-tolerant n-sets) */
/* #define DETAILED_TIME */

/* GNUPLOT modifies the way NUMERIC_PRECISION, NB_OF_CLOSED_N_SETS, NB_OF_LEFT_NODES, TIME, DETAILED_TIME and MIN_SIZE_ELEMENT_PRUNING_TIME (in this order) format their outputs. Instead of being human readable, they are directly understandable by the famous gnuplot software. */
/* #define GNUPLOT */

// Assert
/* ASSERT is used to check the correctness of the computed noise counters. */
/* #define ASSERT */

/* #define DEBUG_HA */

#endif /*PARAMETERS_H_*/
