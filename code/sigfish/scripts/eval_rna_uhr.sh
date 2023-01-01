#!/bin/bash

set -e

HARU_VENV=~/haru/
REF=test/uhr/gencode.v40.10p.ref.fa
BLOW5=test/uhr/reads_4000.blow5
THREADS=8
REF_PAF=test/uhr/rna.minimap2.paf
MY_PAF=test/uhr/test.paf

#minimap2 -t 8 -cx splice -uf -k14 gencode.v40.10p.ref.fa reads_4000.fastq > rna.minimap2.paf

make
#./sigfish dtw ${REF} ${BLOW5} -t ${THREADS} --rna --from-end -q 500 > ${MY_PAF}
#./sigfish dtw ${REF} ${BLOW5} -t ${THREADS} --rna --full-ref --from-end -q 500 > ${MY_PAF}
#./sigfish dtw ${REF} ${BLOW5} -t ${THREADS} --rna --full-ref -q 500  > ${MY_PAF}
./sigfish dtw ${REF} ${BLOW5} -t ${THREADS} --rna -q 250  -p -1 > ${MY_PAF}
#./sigfish dtw ${REF} ${BLOW5} -t ${THREADS} --rna --full-ref -q 500 -p -1 > ${MY_PAF}


source ${HARU_VENV}/bin/activate
uncalled pafstats -r ${REF_PAF} ${MY_PAF} -a > test/uhr/err.paf
./sigfish eval ${REF_PAF} ${MY_PAF} --tid-only