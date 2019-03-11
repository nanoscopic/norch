#include "line_matcher.h"
#include<stdio.h>
#include<stdlib.h>
#include"misc.h"
#include"subreg.h"

line_matcher *line_matcher__new() {
    line_matcher *self = calloc( sizeof( line_matcher ), 1 );
    self->num = 0;
    return self;
}

void line_matcher__add_exp( line_matcher *self, char *exp, int matchCount ) {
    int num = 1;
    if( self->num != 0 ) {
        while( self->next ) {
            num++;
            self = self->next;
        }
        self->next = line_matcher__new();
        self = self->next;
    }
    self->exp = exp;
    self->num = num;
    self->matchCount = matchCount;
}

matched_line *line_matcher__match( line_matcher *self, split_line *line1 ) {
    int numPatterns = 1;
    line_matcher *curMatcher = self;
    while( curMatcher ) {
        numPatterns++;
        curMatcher = curMatcher->next;
    }
    matched_line **blankMatches = (matched_line **) calloc( sizeof( matched_line * ) * numPatterns, 1 );
    
    int i = 0;
    curMatcher = self;
    while( curMatcher ) {
        matched_line *match = blankMatches[ i ] = calloc( sizeof( struct matched_line_s ), 1 );
        printf("Match count: %i\n", curMatcher->matchCount );
        match->captures = calloc( sizeof( subreg_capture_t ) * ( curMatcher->matchCount + 1 ), 1 );
        match->matchCount = curMatcher->matchCount;
        match->patternNum = curMatcher->num;
        i++;
        curMatcher = curMatcher->next;
    }
    
    matched_line *firstResult = NULL;
    matched_line *lastResult = NULL;
    
    // loop through all the lines
    split_line *curLine = line1;
    while( curLine ) {
        curMatcher = self;
        int i = 0;
        char *line = curLine->line;
        //printf("Checking line:%s\n", line );
        while( curMatcher ) {
            matched_line *match = blankMatches[ i ];
            //printf("  Checking matcher i:%i, n:%i\n", i, curMatcher->num );
            subreg_capture_t *captures = match->captures;
            int matchCount = match->matchCount;
            int mres = subreg_match( "\\s+Active: (\\S+).+", line, captures, matchCount+1, 4 );
            if( mres > 0 ) {
                printf("Found a match\n");
                // store the matched_line in the result set
                if( !firstResult ) {
                    firstResult = lastResult = match;
                }
                else {
                    lastResult->next = match;
                    lastResult = match;
                }
                
                // add a new blankMatch into blankMatches
                int matchCount = match->matchCount;
                match = blankMatches[ i ] = calloc( sizeof( struct matched_line_s ), 1 );
                match->captures = calloc( sizeof( subreg_capture_t ) * matchCount, 1 );
                match->matchCount = curMatcher->matchCount;
                match->patternNum = curMatcher->num;
            }
            i++;
            curMatcher = curMatcher->next;
        }
        curLine = curLine->next;
    }
    
    // free the blank matches
    for( int i=0; i<numPatterns; i++ ) {
        matched_line__delete( blankMatches[ i ] );
    }
    
    return firstResult;
}

void matched_line__delete( matched_line *self ) {
    //free( self->captures );
    //free( self );
}