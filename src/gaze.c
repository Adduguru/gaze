/*  Last edited: Oct 10 17:20 2001 (klh) */
/**********************************************************************
 ** File: gaze.c
 ** Author : Kevin Howe
 ** E-mail : klh@sanger.ac.uk
 ** Description : 
 **********************************************************************/


#include "gaze.h"


static char gaze_usage_string[] = "\
Usage: gaze <options>\n\
Options are:\n\
 -structure_file     XML file containing the gaze structure\n\
 -begin_dna <n>      residue number to start looking for genes (def: 1)\n\
 -end_dna <n>        residue number to stop looking for genes (def: sequence length)\n\
 -offset_dna <n>     residue number of the first residue in the DNA file (def: 1)\n\
 -dna_file <s>       file containing the DNA sequence\n\
 -features_file <s>  name of the GFF file containing the features\n\
 -trace_file <s>     name of trace file (def: stderr)\n\
 -output_file <s>    name of output file (def: stdout)\n\
 -path <s>           output the score and probability of the given path\n\
 -defaults <s>       name of the file of defaults (def: './gaze.defaults')\n\
 -selected           look out for Selected features in input\n\
 -help               show this message\n\
 -trace <n>          Print out a trace to the given trace file (n gives detail level)\n\
 -verbose            write basic progess information to stderr\n\
 -post_probs <n>     calculate and show posterior probabilities for features scoring above given\n\
 -no_path            do not print out best path (usually used with -post_probs)\n\
 -full_calc          perform full dynamic programming (as opposed to faster heurstic method\n\
 -sample_gene        calculate and show sampled gene\n";


static Option options[] = {
  { "-begin_dna", INT_ARG },
  { "-end_dna", INT_ARG },
  { "-offset_dna", INT_ARG },
  { "-dna_file", STRING_ARG },
  { "-structure_file", STRING_ARG },
  { "-feature_file", STRING_ARG },
  { "-trace_file", STRING_ARG },
  { "-output_file", STRING_ARG },
  { "-path", STRING_ARG },
  { "-defaults_file", STRING_ARG },
  { "-selected", NO_ARGS },
  { "-help", NO_ARGS },
  { "-verbose", NO_ARGS },
  { "-trace", INT_ARG },
  { "-post_probs", STRING_ARG },
  { "-no_path", NO_ARGS },
  { "-full_calc", NO_ARGS },
  { "-sample_gene", NO_ARGS },
  { "-sigma", FLOAT_ARG }
};


static struct {
  int begin_dna;
  int end_dna;
  int offset_dna;
  double sigma;
  char *structure_file_name;
  FILE *structure_file;
  GArray *feature_file_names; /* of string */
  GArray *feature_files;      /* of FILE */
  char *dna_file_name;
  FILE *dna_file;
  char *trace_file_name;
  FILE *trace_file;
  char *output_file_name;
  FILE *output_file;
  char *path_file_name;
  FILE *path_file;
  int trace;
  gboolean full_calc;
  gboolean use_selected;
  gboolean verbose;
  gboolean post_probs;
  gboolean no_path;
  double post_prob_thresh;
  gboolean sample_gene;
} gaze_options;



/*********************************************************************
 FUNCTION: process_Gaze_Options
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
static gboolean process_Gaze_Options(char *optname,
				     char *optarg ) {
  int i;
  gboolean options_error = FALSE;

  if (strcmp(optname, "-begin_dna") == 0) gaze_options.begin_dna = atoi( optarg );
  else if (strcmp(optname, "-end_dna") == 0) gaze_options.end_dna = atoi( optarg );	     
  else if (strcmp(optname, "-offset_dna") == 0) gaze_options.offset_dna = atoi( optarg );
  else if (strcmp(optname, "-trace") == 0) gaze_options.trace = atoi( optarg );
  else if (strcmp(optname, "-sigma") == 0) gaze_options.sigma = atof( optarg );
  else if (strcmp(optname, "-selected") == 0) gaze_options.use_selected = TRUE;	     
  else if (strcmp(optname, "-verbose") == 0) gaze_options.verbose = TRUE;
  else if (strcmp(optname, "-no_path") == 0) gaze_options.no_path = TRUE;
  else if (strcmp(optname, "-post_probs") == 0) {
    gaze_options.post_probs = TRUE;
    gaze_options.post_prob_thresh = atof( optarg );
  }
  else if (strcmp(optname, "-full_calc") == 0) gaze_options.full_calc = TRUE;
  else if (strcmp(optname, "-sample_gene") == 0) gaze_options.sample_gene = TRUE;
  else if (strcmp(optname, "-trace_file") == 0)  {
    if ((gaze_options.trace_file = fopen( optarg, "w")) == NULL) {
      fprintf( stderr, "Could not open trace file %s for writing\n", optarg );
      options_error = TRUE;
    }
    else {
      if (gaze_options.trace_file_name != NULL)
	g_free( gaze_options.trace_file_name );
      gaze_options.trace_file_name = g_strdup( optarg );
    }
  }
  else if (strcmp(optname, "-output_file") == 0) {
    if ((gaze_options.output_file = fopen( optarg, "w")) == NULL) {
      fprintf( stderr, "Could not open output file %s for writing\n", optarg );
      options_error = TRUE;
    }
    else {
      if (gaze_options.output_file_name != NULL)
	g_free( gaze_options.output_file_name );
      gaze_options.output_file_name = g_strdup( optarg );
    }
  }
  else if (strcmp(optname, "-dna_file") == 0) {
    if ((gaze_options.dna_file = fopen( optarg, "r")) == NULL) {
      fprintf( stderr, "Could not open dna file %s for reading\n", optarg );
      options_error = TRUE;
    }
    else {
      if (gaze_options.dna_file_name != NULL)
	g_free( gaze_options.dna_file_name );
      gaze_options.dna_file_name = g_strdup( optarg );
    }
  }
  else if (strcmp(optname, "-structure_file") == 0) {
    if ((gaze_options.structure_file = fopen( optarg, "r")) == NULL) {
      fprintf( stderr, "Could not open structure file %s for reading\n", optarg );
      options_error = TRUE;
    }
    else {
      if (gaze_options.structure_file_name != NULL)
	g_free( gaze_options.structure_file_name );
      gaze_options.structure_file_name = g_strdup( optarg );
    }
  }
  else if (strcmp(optname, "-feature_file") == 0) {
    /* I allow multiple gff files to be specified. So, fo each one, 
       we have to check that it hasn't already been opened, and
       if it hasn't, open it and store it in the file name list */
    gboolean match = FALSE;
    for (i=0; i < gaze_options.feature_file_names->len; i++) {
      if (! strcmp( optarg, g_array_index( gaze_options.feature_file_names, char *, i)))
	match = TRUE;
    }
    
    if (match)
      /* need to warn about duplicate feature file */
	fprintf( stderr, "Warning: feature file %s was given more than once\n", optarg );
    else {
      /* Try to open the file, and if success, store both the file name
	 and the file handle */
      FILE *tmp_f = fopen( optarg, "r");
      if (tmp_f == NULL) {
	fprintf( stderr, "Could not open feature file %s for reading\n", optarg );
	options_error = TRUE;
      }
      else {
	char *tmp_f_name = g_strdup( optarg );
	g_array_append_val( gaze_options.feature_file_names, tmp_f_name );
	g_array_append_val( gaze_options.feature_files, tmp_f );
      }
    } 
  }
  else if (strcmp(optname, "-path") == 0) {
    if ((gaze_options.path_file = fopen( optarg, "r")) == NULL) {
      fprintf( stderr, "Could not open path file %s for reading\n", optarg );
      options_error = TRUE;
    }
    else {
      if (gaze_options.path_file_name != NULL)
	g_free( gaze_options.path_file_name );
      gaze_options.path_file_name = g_strdup( optarg );
    }
  }
  /* else do nothing */

  return options_error;
}



/*********************************************************************
 FUNCTION: parse_command_line
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
static int parse_command_line( int argc, char *argv[] ) {
  gboolean options_error = FALSE;
  gboolean help_wanted = FALSE;
  int optindex;
  char *optname, *optarg;
  FILE *defaults_fh = NULL;

  while (get_option( argc, argv, options,
		     sizeof(options) / sizeof( Option ),
		     &optindex, &optname, &optarg, &options_error )){
    if (strcmp(optname, "-help") == 0) {
      help_wanted = TRUE;
    }
  }

  if (optindex != argc)
    options_error = TRUE;

  if (help_wanted || options_error) {
    fprintf( stderr, gaze_usage_string );
    return 0;
  }

  /* at this point, we can be sure that all options on the
     command line are valid, so we need not check 
     options_error any more */

  while (get_option( argc, argv, options,
		     sizeof(options) / sizeof( Option ),
		     &optindex, &optname, &optarg, &options_error )){
    if (strcmp(optname, "-defaults_file") == 0) {
      /* open the defaults file and process it */
      if ((defaults_fh = fopen( optarg, "r" )) == NULL) {
	fprintf(stderr,  "Could not open defaults file %s for reading\n", optarg );
	return 0;
      }
    }
  }

  if (defaults_fh == NULL) {
    defaults_fh = fopen( "defaults.gaze", "r" );
  } 

  gaze_options.begin_dna = 1;
  gaze_options.end_dna = 300000000;   /* need to do something about this */
  gaze_options.offset_dna = 1;
  gaze_options.sigma = 1.0;
  gaze_options.dna_file_name = NULL;
  gaze_options.dna_file = NULL;
  gaze_options.feature_file_names = g_array_new( FALSE, TRUE, sizeof( char *) );
  gaze_options.feature_files = g_array_new( FALSE, TRUE, sizeof( FILE *) );
  gaze_options.structure_file_name = NULL;
  gaze_options.structure_file = NULL;
  gaze_options.trace_file_name = g_strdup( "stderr");
  gaze_options.trace_file = stderr;
  gaze_options.output_file_name = g_strdup( "stdout ");
  gaze_options.output_file = stdout;
  gaze_options.path_file_name = NULL;
  gaze_options.path_file = NULL;;
  gaze_options.trace = 0;
  gaze_options.use_selected = FALSE;
  gaze_options.verbose = FALSE;
  gaze_options.full_calc = FALSE;
  gaze_options.post_probs = FALSE;
  gaze_options.post_prob_thresh = 0.0;
  gaze_options.no_path = FALSE;
  gaze_options.sample_gene = FALSE;

  if (process_default_Options( defaults_fh, &process_Gaze_Options  )) {
    return 0;
  }

  /* anything left on the command line has priority, so will
     overwrite settings made so far */


  while (! options_error && get_option( argc, argv, options,
					sizeof(options) / sizeof( Option ),
					&optindex, &optname, &optarg, 
					&options_error )) {

    options_error = process_Gaze_Options( optname, optarg );
  }
  /* check that compulsory args were actually given */

  if (! options_error ) {
    if (gaze_options.structure_file == NULL) {
      fprintf( stderr, "You have not specified a structure file\n");
      options_error = TRUE;
    }
    else if (gaze_options.dna_file == NULL) {
      fprintf( stderr, "Warning: You have not specified a DNA file\n");
    }
    else if (gaze_options.feature_files->len == 0) {
      fprintf( stderr, "You have not specified a GFF feature file\n");
      options_error = TRUE;
    }
    else if (gaze_options.begin_dna > gaze_options.end_dna) {
      fprintf( stderr, "You have given an illegal DNA start/end range\n");
      options_error = TRUE;
    }
  }

  return (!options_error);
}



/*********************************************************************
                        MAIN
 *********************************************************************/

int main (int argc, char *argv[]) {

  Gaze_Structure *gs;
  GArray *features, *segments, *feature_path, *min_scores;
  Feature *beg_ft, *end_ft;
  char *seq_name, *dna_seq;
  int i,j,k;
  int num_segs = 0;
  enum DP_Calc_Mode calc_mode;

  if (! parse_command_line(argc, argv) )
    exit(1);
  
  if(gaze_options.verbose)
    fprintf(stderr, "Parsing structure file\n");
  
  if ((gs = parse_Gaze_Structure( gaze_options.structure_file )) == NULL)
    exit(1);

  /*
  for(i=0; i < gs->length_funcs->len; i++) {
    Length_Function *lf = g_array_index( gs->length_funcs, Length_Function *, i );
    fprintf(stderr, "FUNCTION %s\n", g_array_index( gs->len_fun_dict, char *, i ));
    for(j=0; j < lf->value_map->len; j++) 
      fprintf(stderr, "%7d %.6f\n", j, g_array_index( lf->value_map, double, j ));
  }
  */

  /*
  if (gaze_options.trace > 1)
    print_Gaze_Structure( gs, gaze_options.trace_file );
  */

  features = g_array_new( FALSE, TRUE, sizeof(Feature *));

  segments = g_array_new( FALSE, TRUE, sizeof(Segment_lists *));
  g_array_set_size( segments, gs->seg_dict->len );

  for(i=0; i < segments->len; i++) 
    g_array_index( segments, Segment_lists *, i) = new_Segment_lists(); 

  /* need to record the minimum score seen for each feature type, so 
     that we can obtain a score for features extracted from the DNA */

  min_scores = g_array_new( FALSE, TRUE, sizeof(double) );
  g_array_set_size( min_scores, gs->feat_dict->len );
  for(i=0; i < min_scores->len; i++)
    g_array_index( min_scores, double, i ) = 0.0;

  /* get all the features, starting by adding BEGIN and END by hand */ 
  
  beg_ft = new_Feature();
  beg_ft->feat_idx = dict_lookup( gs->feat_dict, "BEGIN" );
  beg_ft->real_pos.s = gaze_options.begin_dna;
  beg_ft->real_pos.e = gaze_options.begin_dna;
  g_array_append_val( features, beg_ft );
  
  end_ft = new_Feature();
  end_ft->feat_idx = dict_lookup( gs->feat_dict, "END" );
  end_ft->real_pos.s = gaze_options.end_dna;
  end_ft->real_pos.e = gaze_options.end_dna;
  g_array_append_val( features, end_ft );

  /* Get the features from the GFF files... */
  
  if (gaze_options.verbose)
    fprintf(stderr, "Reading the gff files...\n");
  seq_name = get_features_from_gff( gaze_options.feature_files, features, segments, 
				    gs->gff_to_feats, min_scores, 
				    gaze_options.begin_dna, gaze_options.end_dna, 
				    gaze_options.use_selected ); 
  if (seq_name == NULL)
    exit(1);

  /* ...and from the DNA files */

  if (gaze_options.dna_file != NULL) {
    if (gaze_options.verbose)
      fprintf(stderr, "Reading the dna file...\n");
    dna_seq = read_dna_seq( gaze_options.dna_file, 
			    gaze_options.begin_dna, gaze_options.end_dna, gaze_options.offset_dna );
    get_features_from_dna( dna_seq, features, segments, 
			   gs->dna_to_feats, min_scores,
			   gaze_options.begin_dna); 
    if (gs->take_dna != NULL) {
      if (gaze_options.verbose)
	fprintf(stderr, "Getting dna for features...\n");
      get_dna_for_features( dna_seq, features, gs->take_dna, gs->motif_dict,
			    gaze_options.begin_dna, gaze_options.end_dna );
    }
    g_free( dna_seq );
  }
  
  g_array_free( min_scores, TRUE );

  /* All features now obtained. We need to scale, sort, and remove the duplicates */

  if (gaze_options.verbose)
    fprintf(stderr, "Features: sorting, scaling %d feats and removing duplicates...", features->len );

  for( i=0; i < features->len; i++ ) {
    Feature *ft = g_array_index( features, Feature *, i );
    ft->score *= g_array_index( gs->feat_info, Feature_Info *, ft->feat_idx )->multiplier;
    ft->score *= gaze_options.sigma;

    /* for sorting, we need to calculate the effective "position" of each feature, 
       using the start_offset and end_offset */
    ft->adj_pos.s = ft->real_pos.s 
      + g_array_index( gs->feat_info, Feature_Info *, ft->feat_idx )->start_offset;

    ft->adj_pos.e = ft->real_pos.e 
      - g_array_index( gs->feat_info, Feature_Info *, ft->feat_idx )->end_offset;
  }

  qsort( features->data, features->len, sizeof(Feature *), &order_features_forwards); 
  features = remove_duplicate_features( features );

  if (gaze_options.verbose)
    fprintf(stderr, "%d features left\n", features->len);


  /************** print features for debug purposes ****************/
  /*
  if (gaze_options.verbose) {
    for (i=0; i < features->len; i++) 
      print_Feature( stderr, g_array_index( features, Feature *, i), gs->feat_dict, gs->motif_dict); 
    fprintf(stderr, "\n");
  }
  */
  /*******************************************************************/


  /* apply segment score scalings, and sort and index them at the same time */

  for (i=0; i < segments->len; i++) {
    Segment_lists *sl = g_array_index( segments, Segment_lists *, i );
    num_segs += g_array_index( sl->orig, GArray *, 3)->len;
  }

  if (gaze_options.verbose)
    fprintf(stderr, "Segments: scale, sort, project and index %d segments...\n", num_segs);

  for( i=0; i < segments->len; i++ ) {
    double multiplier = g_array_index( gs->seg_info, Segment_Info *, i )->multiplier;
    Segment_lists *seg_lists = g_array_index( segments, Segment_lists *, i);
    
    for (j=0; j < seg_lists->orig->len; j++) {
      GArray *o = g_array_index( seg_lists->orig, GArray *, j);
      GArray *p;
      
      for (k=0; k < o->len; k++) {
	Segment *seg = g_array_index( o, Segment *, k );
	seg->score *= multiplier;
	seg->score *= gaze_options.sigma;
      }
      
      qsort( o->data, o->len, sizeof(Segment *), &order_segments); 
      index_Segments( o );
      
      p = project_Segments( o );
      /* index_Segments should return a sorted list, by construction method */
      index_Segments( p );

      g_array_index( seg_lists->proj, GArray *, j) = p;
    }      

    /* 4th element gives total projected segs of this type, regardless of frame.
       For segments that are frame-dependent, this total will be wrong, but it's 
       impossible at this stage to work out which segments will be used in a 
       frame-dependent manner and which will not; in fact it is possible for the 
       same segment type to be used in both ways. Therefore, this number is just
       an idea of the number of segments after projection */
  }

  /**** print segments for debugging purposes ***
  for( i=0; i < segments->len; i++ ) {

    Segment_lists *seg_lists = g_array_index( segments, Segment_lists *, i);

    fprintf(stderr, "Segment type: %d\n", i );

    for (j=0; j < seg_lists->orig->len; j++) {
      GArray *o = g_array_index( seg_lists->orig, GArray *, j);

      fprintf( stderr, "ORIG: Frame %d, num %d\n", j, o->len );      
      for(k=0; k < o->len; k++) 
	print_Segment(g_array_index(o, Segment *, k), stderr, gs->seg_dict);

    }
    fprintf( stderr, "\n");

    for (j=0; j < seg_lists->proj->len; j++) {
      GArray *p = g_array_index( seg_lists->proj, GArray *, j);

      fprintf(stderr, "PROJ: Frame %d, num %d\n", j, p->len );
      for(k=0; k < p->len; k++)
	print_Segment(g_array_index(p, Segment *, k), stderr, gs->seg_dict);
    }
    fprintf( stderr, "\n\n");
  }
  **********************************************/

  /* Scale the length penalties */

  for(i=0; i < gs->length_funcs->len; i++) {
    Length_Function *lf = g_array_index( gs->length_funcs, Length_Function *, i );
    for( j=0; j < lf->value_map->len; j++) {
      g_array_index( lf->value_map, double, j ) = 
	g_array_index( lf->value_map, double, j ) * lf->multiplier;
      g_array_index( lf->value_map, double, j ) = 
	g_array_index( lf->value_map, double, j ) * gaze_options.sigma;
    }
  }


  /*****************************************************************/
  /*********** finally, do the dynamic programming *****************/
  /*****************************************************************/
  calc_mode = (gaze_options.full_calc)?STANDARD_SUM:PRUNED_SUM;

  /*
  if (gaze_options.begin_dna < 10090706) {
    g_array_remove_index( features, 1 );
    g_array_remove_index( features, 1 );
    g_array_remove_index( features, 1 );
  }
  */

  if (gaze_options.verbose)
    fprintf(stderr, 
	    "Doing forward calculation over %d features and %d segments...\n", 
	    features->len, 
	    num_segs);
  forwards_calc( features, segments, gs, calc_mode, gaze_options.trace, gaze_options.trace_file );

  if ( gaze_options.path_file != NULL) {
    
    if (gaze_options.verbose)
      fprintf(stderr, "Reading the gff correct path file...\n");
    feature_path = read_in_path(gaze_options.path_file, gs->feat_dict, features);
    
    if (feature_path == NULL)
      exit(1);
    
    calculate_path_score( feature_path, segments, gs );
  }
  else {

    if (gaze_options.verbose)
      fprintf(gaze_options.trace_file, "Tracing back...\n");
    feature_path = trace_back_general(features, 
				      segments, 
				      gs, 
				      (gaze_options.sample_gene)?SAMPLE_TRACEBACK:MAX_TRACEBACK ); 
    
    if (gaze_options.post_probs) {
      /*
      if (gaze_options.verbose)
	fprintf(stderr, "Sorting %d features for backward pass...\n", features->len);
      qsort( features->data, features->len, sizeof(Feature *), &order_features_backwards); 
      */
      if (gaze_options.verbose)
	fprintf(stderr, 
		"Doing backward calculation over %d features and %d segments...\n", 
		features->len, 
		num_segs);
      backwards_calc( features, segments, gs, calc_mode, gaze_options.trace, gaze_options.trace_file);
      print_post_probs( gaze_options.output_file, features, gaze_options.post_prob_thresh, gs, seq_name);
    }
  }

  if (! gaze_options.no_path)
    print_GFF_path( gaze_options.output_file, feature_path, gs, seq_name );

  for(i=0; i < features->len; i++)
    free_Feature( g_array_index( features, Feature *, i));
  g_array_free( features, TRUE);
  for(i=0; i < segments->len; i++)
    free_Segment_lists( g_array_index( segments, Segment_lists *, i ));
  g_array_free( segments, TRUE);
  g_free( seq_name );
  g_array_free (feature_path, TRUE );
  free_Gaze_Structure( gs );
  
  return 0;
}

