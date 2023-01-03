# sigfish

## Building

Dependencies: zlib development libraries

```
git clone https://github.com/hasindu2008/sigfish
cd sigfish
make
```


## dtw

Performs dynamic time warping-based alignment of raw signals in S/BLOW5 format to a reference in FASTA format. This is an all-to-all alignment and is not intended for large references.

```
Usage: sigfish dtw [OPTIONS] genome.fa reads.blow5
```

Output is in PAF-like format (inspired by UNCALLED) with the following columns

|Col|Type  |Description                               |
|--:|:----:|:-----------------------------------------|
|1  |string|Read identifier name                       |
|2  |int   |Raw signal length (number of samples)                    |
|3  |int   |Raw signal start index  (0-based; BED-like; closed)   |
|4  |int   |Raw signal end index (0-based; BED-like; open)       |
|5  |char  |Relative strand: "+" or "-"               |
|6  |string|Reference name                     |
|7  |int   |Reference sequence length                    |
|8  |int   |start on the reference sequence (0-based; BED-like; closed)  |
|9  |int   |end on reference sequence (0-based; BED-like; open)   |
|10 |int   |Approximation of number of matching bases in the alignment                |
|11 |int   |Alignment block length on the reference in terms of bases                 |
|12 |int   |Mapping quality (0-255; 255 for missing)  |



Following optional tags are present:

|Tag|Type  |Description                               |
|--:|:----:|:-----------------------------------------|
|tp  |A   |Type of alignment: P/primary                      |
|d1  |f   |DTW score (lower the better)                     |
|d2  |f   |DTW score of the next best alignment                       |


## eval

Similar to UNCALLED pafstats.


## Acknowledgement
The event detection code is from Oxford Nanopore's [Scrappie basecaller](https://github.com/nanoporetech/scrappie).
The DTW code is from [mlpy](http://mlpy.sourceforge.net/).
The pore-models are from [Nanopolish](https://github.com/jts/nanopolish).
Some code snippets have been taken from [Minimap2](https://github.com/lh3/minimap2), [Samtools](http://samtools.sourceforge.net/).
