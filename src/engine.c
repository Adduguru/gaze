/**********************************************************************
 ** File: params.c
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
 * Author : Kevin Howe
 * E-mail : klh@sanger.ac.uk
 * Description : Routines for the general dynamic programming engine 
 **********************************************************************/
#include "engine.h"


/*********************************************************************
 FUNCTION: calculate_post_accuracies
 DESCRIPTION:
    This function takes the given list of features, and measures the
    accuracies of the posterior probabilities of each feature. It
    does this by plotting a histogram (with the number of bins given)
    of the proportion of features that are correct for various ranges
    of posterior probability (idea: 70% of features that have a post.
    prob. of 0.7 should be correct).
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
Array *calculate_post_accuracies( Array *feats, int bins, double sigma ) {
  int i;
  Array *res = new_Array( sizeof( double ), TRUE );
  Array *trues = new_Array( sizeof( int ), TRUE );
  Array *totals = new_Array( sizeof( int  ), TRUE );


  set_size_Array( res, bins );
  set_size_Array( trues, bins );
  set_size_Array( totals, bins );

  for(i=0; i < feats->len; i++) {
    Feature *ft = index_Array( feats, Feature *, i);

    double post_prob = exp( ft->forward_score +
			    ft->backward_score - 
			    index_Array( feats, Feature *, 0)->backward_score );
						   
    int index = (int) (post_prob * (double) bins);

    /* the following is to allow for the special case of a post prob of 1 */
    
    if (index == bins)
      index = bins - 1;

    index_Array( totals, int, index ) = index_Array( totals, int, index ) + 1;			    
    if (ft->is_correct) 
      index_Array( trues, int, index ) = index_Array( trues, int, index ) + 1;

  }
    
  for(i=0; i < res->len; i++) {
    if (index_Array( totals, int, i ) > 0)
      index_Array( res, double, i ) = 
	(double) index_Array( trues, int, i ) /
	(double) index_Array( totals, int, i );
  }

  free_Array( trues, TRUE );
  free_Array( totals, TRUE );

  /* put output in here for now. */

  printf( "## Posterior probability accuracy plot - sigma = %.3f\n", sigma );
  printf( "## Post prob     Prop. correct\n" );
  for (i=0; i < res->len; i++) {
    printf( "## %4.3f:%4.3f\t%.3f\n", 
	    1.0 / (double) bins * i, 
	    1.0 / (double) bins * (i+1),
	    index_Array( res, double, i ));
  }

  return res;
}



/*********************************************************************
 FUNCTION: calculate_segment_score
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
double calculate_segment_score(Gaze_Sequence *g_seq, 
			       Feature *src, 
			       Feature *tgt, 
			       Gaze_Structure *gs,
			       Seg_Results *s_res) {
  Array *seg_quals;
  int i,j;
  double score;

  Feature_Info *tgt_info = index_Array( gs->feat_info, 
					Feature_Info *,
					tgt->feat_idx );
  Feature_Relation *tgt_rel = index_Array( tgt_info->sources, Feature_Relation *, src->feat_idx);

  int src_pos = src->adj_pos.s; 
  int tgt_pos = tgt->adj_pos.e; 

  for(i=0; i < s_res->has_score->len; i++) {
    index_Array( s_res->has_score, boolean, i ) = FALSE;
    index_Array( s_res->raw_scores, double, i ) = 0.0;
  }
  s_res->has_exact_at_src = s_res->exact_extends_beyond_tgt = FALSE;
  s_res->has_exact_at_tgt = s_res->exact_extends_beyond_src = FALSE;

  if ( (seg_quals = tgt_rel->seg_quals) != NULL) {
    for (i=0; i < seg_quals->len; i++) {
      Segment_Qualifier *qual = index_Array( seg_quals, Segment_Qualifier *, i);

      if (qual != NULL) {
	int index;
	Array *segs, *list;

	Segment_list *sl = index_Array( g_seq->segment_lists, Segment_list *, qual->seg_idx);

	if (qual->has_tgt_phase)
	  index = (tgt_pos - qual->phase + 1) % 3;
	else if (qual->has_src_phase) 
	  index = (src_pos + qual->phase) % 3 ;
	else 
	  index = 3;

	/********************************************************************/
	/*
	  the following lines utilise per_segment cumulative scoring. Note 
	  the + 5; this was because I was using base scores that were pobabilities
	  given the previous 5 bases; To ise these scores for the first 5 bases of
	  a candidate exon would be wrong, because the the previous 5 bases run 
	  into non-coding DNA. The proper way to do this would be to have a 5mer
	  probability for the start of each exon.
	*/
	/*
	index_Array( s_res->raw_scores, double, qual->seg_idx ) = 
	  sl->per_base[index][tgt_pos - g_seq->seq_region.s] 
	  - sl->per_base[index][src_pos - g_seq->seq_region.s - 1 + 5]; 

	index_Array( s_res->has_score, boolean, qual->seg_idx ) = TRUE;
	continue;
	*/
	/***********************************************************************/

	list = (qual->use_projected) ? sl->proj : sl->orig;
	segs = index_Array( list, Array *, index );

	{
	  /* find min j s.t. segs[j].pos.s > end_pos */
	  /* strategy: binary search */

	  int left = 0;
	  int right = segs->len;

	  while (left < right) {
	    int mid = (left + right) / 2;

	    if (index_Array( segs, Segment *, mid)->pos.s <= tgt_pos)
	      left = mid + 1;
	    else 
	      right = mid;
	  }
	  j = left - 1;
	}

	for (; j >= 0; j--) {
	  Segment *seg = index_Array( segs, Segment *, j ); 

	  if ( seg->max_end_up < src_pos )
	    break;
	  else if ( seg->pos.e < src_pos )
	    continue;
	  else {
	    int low = (seg->pos.s < src_pos)?src_pos:seg->pos.s;
	    int high = (seg->pos.e > tgt_pos)?tgt_pos:seg->pos.e;
	  
	    /* the following block passes exact segment information
	       back to the caller, to help with decisions about pruning */
	    if (qual->is_exact_src && seg->pos.s == src_pos ) {
	      s_res->has_exact_at_src = TRUE;
	      if (seg->pos.e > tgt_pos)
		s_res->exact_extends_beyond_tgt = TRUE;
	    }
	    if (qual->is_exact_tgt && seg->pos.e == tgt_pos) {
	      s_res->has_exact_at_tgt = TRUE;
	      if (seg->pos.s < src_pos)
		s_res->exact_extends_beyond_src = TRUE;
	    }
	   
	    if ((! qual->is_exact_src || seg->pos.s == src_pos ) &&
		(! qual->is_exact_tgt || seg->pos.e == tgt_pos)) {
	  
	      if (qual->partial || (seg->pos.s >= src_pos && seg->pos.e <= tgt_pos)) {
		/* note 020128 - now scores are per-residue, can get scores without a division op */
		/* score = seg->score * ( (double)(high - low + 1) / (double) (seg->pos.e - seg->pos.s + 1)); */
		
		score = seg->score * (high - low + 1);
		
		if (! index_Array( s_res->has_score, boolean, qual->seg_idx)) {
		  index_Array( s_res->raw_scores, double, qual->seg_idx) = score;
		  index_Array( s_res->has_score, boolean, qual->seg_idx) = TRUE;
		  
		}
		else {
		  if (qual->score_sum)
		    /* now sum projected segment scores in a region rather than take max */
		    index_Array( s_res->raw_scores, double, qual->seg_idx) += score;		  
		  else {
		    if (score > index_Array( s_res->raw_scores, double, qual->seg_idx )) {
		      index_Array( s_res->raw_scores, double, qual->seg_idx) = score;
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }

  score = 0.0;
  for(i=0; i < s_res->has_score->len; i++)
    if (index_Array( s_res->has_score, boolean, i))
	score += index_Array( s_res->raw_scores, double, i );
     
  return score;
}


/*********************************************************************
 FUNCTION: is_legal_path
 DESCRIPTION:
   Returns true iff the given list of features, when interpreted as 
   a path, is legal with repect to the give structure
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
boolean is_legal_path( Array *path,
		       Gaze_Structure *gs ) {

  int idx, left_pos, right_pos, distance, k; 
  Feature_Info *src_info, *tgt_info;
  Feature_Relation *reg_info;
  Feature *src, *tgt;
  boolean legal_path = TRUE;

  for( idx=0; legal_path && idx < path->len - 1; idx++) { 
    /* for the score to mean anything, all paths must begin with "BEGIN"
       and end with "END." Therefore ignoring the local score of the first 
       feature (done here) has no effect, since the score of "BEGIN" is 0
       (and has to be for the DP to work */

    src = index_Array( path, Feature *, idx );
    tgt = index_Array( path, Feature *, idx + 1 );

    src_info = index_Array( gs->feat_info, Feature_Info *, src->feat_idx );
    tgt_info = index_Array( gs->feat_info, Feature_Info *, tgt->feat_idx );

    left_pos = src->real_pos.s + src_info->start_offset;
    right_pos = tgt->real_pos.e - tgt_info->end_offset;
    
    distance = right_pos - left_pos + 1;

    if ((reg_info = index_Array(tgt_info->sources, Feature_Relation *, src->feat_idx)) != NULL) {
      
      if ((reg_info->phase == NULL) || (*(reg_info->phase) == distance % 3)) {
	
	if ((reg_info->min_dist == NULL) || (*(reg_info->min_dist)) <= distance) {
	  
	  if ((reg_info->max_dist == NULL) || (*(reg_info->max_dist)) >= distance) {
	    /* check for DNA killers */
	    boolean killed_by_dna = FALSE;

	    if (reg_info->kill_dna_quals != NULL) {
	      for(k=0; k < reg_info->kill_dna_quals->len; k++) {
		Killer_DNA_Qualifier *kdq = index_Array( reg_info->kill_dna_quals, 
							   Killer_DNA_Qualifier *,
							   k );

		if (src->dna > 0 && src->dna == kdq->src_dna 
		    && tgt->dna > 0 && tgt->dna == kdq->tgt_dna) { 
		  
		  killed_by_dna = TRUE;
		}
	      }
	    }

	    if (! killed_by_dna) 
	      continue;
	    else { 
	      fprintf(stderr, "The given path is illegal due to DNA constraints\n"); 
	      legal_path = FALSE;
	    }
	  }
	  else {
	    fprintf( stderr, "The given path is illegal to a maximum distance violation\n" ); 
	    legal_path = FALSE;
	  }
	}
	else {
	  fprintf( stderr, "The given path is illegal to a minimum distance violation\n" );
	  legal_path = FALSE;	    
	}
      }
      else {
	fprintf( stderr, "The given path is illegal to a phase violation\n" );
	legal_path = FALSE;
      }
    }
    else {
      fprintf( stderr, "The given path has an illegal pair of features (%s, %s)\n",
	       index_Array( gs->feat_dict, char *, src->feat_idx), 
	       index_Array( gs->feat_dict, char *, tgt->feat_idx) ); 
      legal_path = FALSE;
    }
  }

  return legal_path;
}


/*********************** Index_list *********************************/


/*********************************************************************
 FUNCTION: add_to_Index_list
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
Index_list *add_to_Index_list( Index_list *l, int idx, boolean keep) {
  Index_list *idxlist = new_Index_list();

  idxlist->idx = idx;
  idxlist->need_to_keep = keep;
  idxlist->next = l;

  return idxlist;
}


/*********************************************************************
 FUNCTION: free_Index_list
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
Index_list *free_Index_list( Index_list *l, boolean keep) {

  /* progress to the first node we wish to keep */
  while (l != NULL && (!keep || ! l->need_to_keep)) {
    Index_list *temp = l;
    l = temp->next;
    free_util(temp);
  } 

  /* now process rest of list */
  if (l != NULL) {
    Index_list *current_tail = l;
    Index_list *current = l->next;
    current_tail->need_to_keep = FALSE;

    while( current != NULL ) {
      if (keep && current->need_to_keep) {
	current->need_to_keep = FALSE;
	current_tail->next = current;
	current_tail = current;
	current = current_tail->next;
      }
      else {
	Index_list *temp = current;
	current = temp->next;
	free_util(temp);
	current_tail->next = NULL;
      }
    }
  }

  return l;
    
}



/*********************************************************************
 FUNCTION: new_Index_list
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
Index_list *new_Index_list(void) {
  Index_list *l = (Index_list *) malloc_util( sizeof(Index_list) );

  l->need_to_keep = FALSE;
  l->idx = 0;
  l->next = NULL;

  return l;
}



/*********************************************************************
 FUNCTION: traverse_Index_list
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
void traverse_Index_list( Index_list *l ) {

  while (l != NULL) {
    fprintf(stderr, "IDX=%d ", l->idx);

    if (l->need_to_keep) {
      fprintf(stderr, " KEEP ");
    }
    l = l->next;
  }
  fprintf(stderr, "\n");
}




/*********************** Seg_Results *********************************/

/*********************************************************************
 FUNCTION: free_Seg_Results
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
void free_Seg_Results( Seg_Results *s_res ) {
  if (s_res != NULL) { 
    free_Array( s_res->raw_scores, TRUE );
    free_Array( s_res->has_score, TRUE );
    free_util( s_res );
  }
}


/*********************************************************************
 FUNCTION: new_Seg_Results
 DESCRIPTION:
 RETURNS:
 ARGS: 
 NOTES:
 *********************************************************************/
Seg_Results *new_Seg_Results( int seg_dict_size ) {
  Seg_Results *s_res;

  s_res = (Seg_Results *) malloc_util(sizeof(Seg_Results));
  s_res->raw_scores = new_Array( sizeof( double ), TRUE ); 
  s_res->has_score = new_Array( sizeof( boolean ), TRUE ); 
  set_size_Array( s_res->raw_scores, seg_dict_size );
  set_size_Array( s_res->has_score, seg_dict_size );
  s_res->has_exact_at_src = s_res->exact_extends_beyond_tgt = FALSE;
  s_res->has_exact_at_tgt = s_res->exact_extends_beyond_src = FALSE;

  return s_res;
}
