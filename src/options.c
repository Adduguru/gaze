/**********************************************************************
 * File: options.c
 * Author: Kevin Howe
 * Copyright (C) Genome Research Limited, 2002-
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * DESCRIPTION:
 * Rudimentary provision of command-line options
 **********************************************************************/
#include "options.h"



/*********************************************************************
 FUNCTION: get_option
 DESCRIPTION: 
   Gets an option from the given command line
 RETURNS:
   1, if a valid option was found
   0, if no valid option was found and option parsing is therefore 
      complete
 ARGS: 
 NOTES: 
*********************************************************************/
int get_option(int argc, 
	       char **argv, 
	       Option *opt, 
	       int num_opts, 
	       int *ret_optindex, 
	       char **ret_optname, 
	       char **ret_optarg,
	       boolean *error) {

  static int optindex = 1;        /* init to 1 on first call  */
  static char *optptr = NULL;     /* ptr to next valid switch */

  unsigned int i, arglen, matches, opti = 0; /* initialised to get around gcc warnings */ 

  /* Check to see if we've run out of options. '-' on its own is not an option */
  
  *error = FALSE;

  if (optindex >= argc || argv[optindex][0] != '-' || strcmp(argv[optindex], "-") == 0) { 
    *ret_optindex  = optindex; 
    *ret_optarg  = NULL; 
    *ret_optname = NULL;
    
    /* reset in preparation for another call - allows us to scan for
       each option individually, in effect prioritising them */
    optindex = 1;
    
    return 0; 
  }
  
  /* We have an option. All options are treated as 'full' optiions,
     including single letter ones, which means you can't join
     single letter options with -abc; use -a -b -c */
  
  if (optptr == NULL && (argv[optindex][0] == '-')) {
    if ((optptr = strchr(argv[optindex], '=')) != NULL) { 
      *optptr = '\0'; 
      optptr++; 
    }
    
    arglen = strlen(argv[optindex]);
    matches = 0;
    for (i = 0; i < num_opts; i++) {
      if (strncmp(opt[i].name, argv[optindex], arglen) == 0) { 
	matches++;
	opti = i;
      }
    }
    if (matches > 1 && arglen != strlen(opt[opti].name)) {
      fprintf(stderr, "Option \'%s\' is ambiguous.\n", argv[optindex]);
      *error = TRUE;
      return 0;
    }
    if (matches == 0) {
      fprintf(stderr, "No such option as \'%s\'.\n", argv[optindex]);
      *error = TRUE;
      return 0;
    }
    
    *ret_optname = opt[opti].name;
    
    if (opt[opti].type != NO_ARGS) {
      if (optptr != NULL) {
	/* option value was given as -opt=val */
	*ret_optarg = optptr;
	optptr = NULL;
	optindex++;
      }
      else if (optindex+1 >= argc) {
	/* no option was given */
	fprintf(stderr, "Option %s needs an argument\n", opt[opti].name);
	*error = TRUE;
	return 0;
      }
      else {
	/* assume that the next thing on the command-line is the option value */
	if (argv[optindex+1][0] != '-') {
	  *ret_optarg = argv[optindex+1];
	  optindex += 2;
	}
	else {
	  fprintf(stderr, "Option %s needs an argument\n", opt[opti].name);
	  *error = TRUE;
	  return 0;
	}
      }
    }
    else {
      if (optptr != NULL) {
	fprintf(stderr, "Option %s does not take any arguments\n", opt[optindex].name);
	*error = TRUE;
	return 0;
      }
      *ret_optarg = NULL;
      optindex++;
    }
  }

  *ret_optindex = optindex;
  return 1;
}





/*********************************************************************
 FUNCTION: process_default_Options
 DESCRIPTION:
   Reads the given file of defaults, assuming the following format:
     optionname1 = value1
     optname2
     optname3 = value3
   and calls the given function to process the option pair read
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
int process_default_Options( FILE *defs, 
			     boolean (*func)(char *, char *) ) {
  char *buffer;
  int options_error = FALSE;
  int start_tag, start_val, i;

  if (defs == NULL)
    return options_error;

  buffer  = (char *) malloc_util(MAX_DEF_LINE_SIZE * sizeof(char));

  while ( (fgets( buffer, MAX_DEF_LINE_SIZE, defs )) != NULL) {
    start_tag = start_val = 0;

    for(i=0; isspace((int)buffer[i]); i++);
    if (buffer[i] == '\0' || buffer[i] == '#') {
      continue;
    }

    start_tag = i;
    for(; ! isspace((int)buffer[i]) && ! (buffer[i] == '='); i++);
    buffer[i] = '\0'; i++;
    for(; isspace((int)buffer[i]) || (buffer[i] == '='); i++);
    start_val = i;
    if (buffer[i]) {
      for(; ! isspace((int)buffer[i]) ; i++);
      buffer[i] = '\0';
    }

    options_error = (*func)((char *) &buffer[start_tag],
			    (char *) &buffer[start_val] ) || options_error;
  }

  free_util(buffer);

  return options_error;
}



