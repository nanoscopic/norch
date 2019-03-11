#ifndef __LINE_MATCHER_H
#define __LINE_MATCHER_H

#include"subreg.h"
#include"misc.h"

typedef struct line_matcher_s line_matcher;
struct line_matcher_s {
    int num;
    char *exp;
    int matchCount;
    line_matcher *next;
};
typedef struct matched_line_s {
    int patternNum; // number of the pattern that matched
    struct matched_line_s *next;
    subreg_capture_t *captures;
    int matchCount;
    //char *line;
    
} matched_line;



line_matcher *line_matcher__new();
void line_matcher__add_exp( line_matcher *self, char *exp, int matchCount );
matched_line *line_matcher__match( line_matcher *self, split_line *line1 );
void matched_line__delete( matched_line *self );

#endif