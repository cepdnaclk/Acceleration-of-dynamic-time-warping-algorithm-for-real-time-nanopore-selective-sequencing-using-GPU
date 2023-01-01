/* @file  eval.c
**
** @@
******************************************************************************/

#define _XOPEN_SOURCE 700
#include <assert.h>
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sigfish.h"
#include "khash.h"
#include "error.h"

#define MAX_ALN 1

static struct option long_options[] = {
    {"verbose", required_argument, 0, 'v'},        //0 verbosity level [1]
    {"help", no_argument, 0, 'h'},                 //1
    {"version", no_argument, 0, 'V'},              //2
    {"output",required_argument, 0, 'o'},          //3 output to a file [stdout]
    {"secondary",required_argument, 0, 0},         //4 consider secondary
    {"tid-only",no_argument, 0, 0},                 //5 consider target name only
    {0, 0, 0, 0}};

typedef struct{
    char *rid;
    int32_t qlen;
    int32_t query_start;
    int32_t query_end;
    int8_t strand;
    char *tid;
    int32_t tlen;
    int32_t target_start;
    int32_t target_end;
    uint8_t mapq;
    char tp;
}paf_rec_t;

typedef struct{
    paf_rec_t **paf;
    int n;
    int c;
}read_hval_t;

typedef struct{
    int8_t sec;
    int8_t tid_only;
}eval_opt_t;


typedef struct{

    int64_t truth_rec;    //number of mapped records
    int64_t test_rec;
    int64_t truth_mapped; //number of reads mapped (exclude multiple alignments)
    int64_t test_mapped;

    //number of reads that fall under each scenario
    //int64_t unmapped_in_both; //the read is unmapped in both truthset and testset
    int64_t correct;             //correct - the read maps to the same location (locations overlaps if overlap based evaluation is set)
    int64_t incorrect;       //incorrect
    //int64_t only_in_a;        //read is only mapped in  truthset
    int64_t only_in_b;        //read is only mapped in  testset
    //int64_t pri_a_to_supp_b;  //primary mapping in truthset is a supplementary mapping in testset
    //int64_t pri_a_to_sec_b;   //primary mapping in testset is a secondary mappings in testset

    int64_t mapq_correct[61];
    int64_t mapq_incorrect[61];

}eval_stat_t;

KHASH_MAP_INIT_STR(eval_s2a, read_hval_t*)
typedef khash_t(eval_s2a) eval_hash_t;


paf_rec_t *parse_paf_rec(char *buffer){

    char *pch=NULL;

    paf_rec_t *paf = (paf_rec_t *)malloc(sizeof(paf_rec_t));
    MALLOC_CHK(paf);

    //read name
    pch = strtok (buffer,"\t\r\n"); assert(pch!=NULL);
    paf->rid = strdup(pch);

    //readlen
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->qlen = atoi(pch);

    //query start
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->query_start = atoi(pch);

    //query end
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->query_end= atoi(pch);

    //relative strand
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    if(strcmp(pch,"+")==0){
        paf->strand=0;
    }
    else if(strcmp(pch,"-")==0){
        paf->strand=1;
    }
    else{
        assert(0);
    }

    //targetname
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->tid = strdup(pch);

    //target len
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->tlen = atoi(pch);

    //target start
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->target_start = atoi(pch);

    //target end
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->target_end= atoi(pch);

    //num residue
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);

    //num block
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);

    //mapq
    pch = strtok (NULL,"\t\r\n"); assert(pch!=NULL);
    paf->mapq = atoi(pch);

    paf->tp = 'P';
    while((pch = strtok(NULL,"\t\r\n"))){
        if(strcmp("tp:A:P",pch)==0){
            paf->tp = 'P';
        }
        else if (strcmp("tp:A:S",pch)==0){
            paf->tp = 'S';
        }
    }

    return paf;
}

void free_paf_rec(paf_rec_t *paf){
    free(paf->rid);
    free(paf->tid);
    free(paf);
}

eval_hash_t* get_truth(FILE *paffile,eval_stat_t *stat){

    //buffers for getline
    size_t bufferSize = 4096;
    char *buffer = (char *)malloc(sizeof(char)*(bufferSize));
    MALLOC_CHK(buffer);
    int readlinebytes=1;
    int mappings_total = 0;

    eval_hash_t *h;
    khint_t k;
    h = kh_init(eval_s2a);

	while(1){
        readlinebytes=getline(&buffer, &bufferSize, paffile);
        if(readlinebytes == -1){
            break;
        }

        paf_rec_t *paf = parse_paf_rec(buffer);

        int absent;
        k = kh_put(eval_s2a, h, paf->rid, &absent);
        if (absent == -1){
            ERROR("Inserting key '%s' into the auxiliary hash table failed.", paf->rid);
            exit(EXIT_FAILURE);
        }
        else if (absent >0 ) {
            read_hval_t *s = (read_hval_t *)malloc(sizeof(read_hval_t));
            MALLOC_CHK(s);
            s->n=1;
            s->c=1;
            s->paf=(paf_rec_t **)malloc(sizeof(paf_rec_t*)*s->c);
            MALLOC_CHK(s);
            s->paf[0]=paf;
            kh_value(h, k) = s;
        } else {
            read_hval_t *s = kh_value(h, k);
            s->c++;
            s->paf=(paf_rec_t **)realloc(s->paf,sizeof(paf_rec_t*)*s->c);
            MALLOC_CHK(s->paf);
            s->paf[s->n]=paf;
            s->n++;
        }

        mappings_total++;
        //free_paf_rec(paf);
    }
    stat->truth_rec= mappings_total;
    stat->truth_mapped=kh_size(h);

    free(buffer);

    return h;

}


#define THRESHOLD 100
int is_correct_overlap(paf_rec_t *a, paf_rec_t *b, eval_opt_t *opt)
{
    if (strcmp(a->tid,b->tid)!=0 || a->strand != b->strand){
        return 0;
    }
    if(opt->tid_only){
        return 1;
    }

    int diff_st = a->target_start - b->target_start;
    diff_st = diff_st < 0 ? -diff_st : diff_st;
    int diff_end = a->target_end - b->target_end;
    diff_end = diff_end < 0 ? -diff_end : diff_end;

    int diff = diff_end<diff_st ? diff_end : diff_st;

    if(diff < THRESHOLD){
        return 1;
    } else {
        return 0;
    }


}

int compare(paf_rec_t *a, paf_rec_t *b, eval_opt_t *opt){

    assert(strcmp(a->rid,b->rid)==0);

    return is_correct_overlap(a,b,opt);

}

void free_hash(eval_hash_t* h){
    khint_t k;
    for (k = kh_begin(h); k != kh_end(h); ++k) {
        if (kh_exist(h, k)) {
            read_hval_t *s = kh_value(h, k);
            int i;
            for(i=0;i<s->n;i++){
                free_paf_rec(s->paf[i]);
            }
            free(s->paf);
            free(s);
        }
    }
    kh_destroy(eval_s2a, h);
}



void parse_eval(FILE *paffile, eval_hash_t* truth, eval_opt_t *opt, eval_stat_t *stat){

    //buffers for getline
    size_t bufferSize = 4096;
    char *buffer = (char *)malloc(sizeof(char)*(bufferSize));
    MALLOC_CHK(buffer);
    int readlinebytes=1;
    int mappings_total = 0;

	while(1){
        readlinebytes=getline(&buffer, &bufferSize, paffile);
        if(readlinebytes == -1){
            break;
        }

        paf_rec_t *paf = parse_paf_rec(buffer);
        eval_hash_t *h = truth;
        khint_t k = kh_get(eval_s2a, h, paf->rid);
        if (k == kh_end(h)){
            //fprintf(stderr,"Error: key '%s' not found.\n", paf->rid);
            stat->only_in_b++;
        }
        else{
            read_hval_t *s = kh_value(h, k);
            int i;
            int ret = 0;

            for(i=0;i<s->n;i++){
                if(opt->sec || s->paf[i]->tp==paf->tp){
                    ret = compare(s->paf[i],paf,opt);
                    if(ret) {
                        break;
                    }
                }
            }
            int mapq = paf->mapq;
            assert(mapq>=0 && mapq<=60);
            if(ret){
                stat->correct++;
                stat->mapq_correct[mapq]++;
            }
            else{
                stat->incorrect++;
                stat->mapq_incorrect[mapq]++;
            }
        }
        free_paf_rec(paf);

        mappings_total++;
    }
    stat->test_rec = mappings_total;
    stat->test_mapped = mappings_total;

    fprintf(stderr,"Total mappings in testset: %d\n",mappings_total);
    free(buffer);


}

void print_compare_stat(eval_stat_t *stat){
    printf("\nComparison between truthset and testset\n"
    "mapped_truthset\t%ld\n"
    "mapped_testset\t%ld (%.2f%%)\n"
    "correct\t%ld (%.2f%%)\n"
    "incorrect\t%ld (%.2f%%)\n"
    //"only_in_truthset\t%ld\n"
    "only_in_testset\t%ld\n",
    stat->truth_mapped, stat->test_mapped,stat->test_mapped/(float)stat->truth_mapped*100 , stat->correct, stat->correct/(float)stat->test_mapped*100,
    stat->incorrect,stat->incorrect/(float)stat->test_mapped*100, stat->only_in_b);
#ifdef CONSIDER_SUPPLEMENTARY
    printf("Primary in 'a' is a supplementary in 'b'\t%d\n",stat->pri_a_to_supp_b);
#endif
#ifdef CONSIDER_SECONDARY
    printf("Primary in 'a' is a secondary in 'b'\t%d\n",stat->pri_a_to_sec_b);
#endif

    printf("\n#mapq\tcorrect\tincorrect\n");
    int i;
    for(i=60;i>=0;i--){
        int c = stat->mapq_correct[i];
        int ic = stat->mapq_incorrect[i];
        if (!(c==0 && ic==0)){
            printf("%d\t%d\t%d\n",i,c,ic);
        }

    }

}

void init_eval_opt(eval_opt_t* opt) {
    memset(opt, 0, sizeof(eval_opt_t));
    opt->sec = 1;
}


int8_t yes_or_no( int long_idx,const char* arg){

    int8_t ret=0;
    if (strcmp(arg, "yes") == 0 || strcmp(arg, "y") == 0) {
        ret=1;
    } else if (strcmp(arg, "no") == 0 || strcmp(arg, "n") == 0) {
        ret=0;
    } else {
        WARNING("option '--%s' only accepts 'yes' or 'no'.",
                long_options[long_idx].name);
    }
    return ret;

}

int eval_main(int argc, char* argv[]) {

    const char* optstring = "o:hV";

    int longindex = 0;
    int32_t c = -1;

    FILE *fp_help = stderr;
    //int8_t rna = 0;
    eval_opt_t opt;
    init_eval_opt(&opt);

    //parse the user args
    while ((c = getopt_long(argc, argv, optstring, long_options, &longindex)) >= 0) {
        if (c=='V'){
            fprintf(stdout,"sigfish %s\n",SIGFISH_VERSION);
            exit(EXIT_SUCCESS);
        } else if (c=='h'){
            fp_help = stdout;
        } else if(c == 0 && longindex == 4){ //secondary mappings
            opt.sec=yes_or_no(longindex, optarg);
        } else if(c == 0 && longindex == 5){ //tid-only
            opt.tid_only = 1;
        }
    }

    if (argc-optind<2 ||  fp_help == stdout) {
        fprintf(fp_help,"Usage: sigfish eval truth.paf test.paf\n");
        fprintf(fp_help,"\nbasic options:\n");
        fprintf(fp_help,"   -h                         help\n");
        fprintf(fp_help,"   --version                  print version\n");
        fprintf(fp_help,"   --secondary STR            consider secondary mappings. yes or no.\n");
        fprintf(fp_help,"   --tid-only                 consider regerence name and strand only\n");
        if(fp_help == stdout){
            exit(EXIT_SUCCESS);
        }
        exit(EXIT_FAILURE);
    }

    FILE *truth = fopen(argv[optind], "r");
    if (truth==NULL) {
        ERROR("cannot open %s. \n", argv[optind]);
        exit(EXIT_FAILURE);
    }

    eval_stat_t stat;
    memset(&stat,0,sizeof(eval_stat_t));

    eval_hash_t* h=get_truth(truth, &stat);
    fclose(truth);

    FILE *test = fopen(argv[optind+1], "r");
    if (test==NULL) {
        ERROR("cannot open %s. \n", argv[optind]);
        exit(EXIT_FAILURE);
    }
    parse_eval(test,h,&opt,&stat);
    free_hash(h);


    fclose(test);

    print_compare_stat(&stat);

    return 0;
}
