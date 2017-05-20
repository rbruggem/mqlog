#ifndef MQLOG_MQLOGERRNO_H_
#define MQLOG_MQLOGERRNO_H_

#define ELALLC   -1 // allocation error
#define ELNOPGM  -2 // size is not a multiple of page size
#define ELSOFLW  -3 // string overflow
#define ELUNKWN  -4 // unkown error
#define ELFLEOP  -5 // file operation error
#define ELFSTAT  -6 // fstat error
#define ELSGRMT  -7 // segment metadata read error
#define ELSGSMT  -8 // segment metadata sync error
#define ELMMAP   -9 // mmap error
#define ELMADV  -10 // madvice error
#define ELNOWCP -11 // no write capacity error
#define ELNORD  -12 // nothing to read
#define ELINVHD -13 // invalid frame error
#define ELLGDIR -14 // log directory error
#define ELLGCLS -15 // closing log error
#define ELLGDTR -16 // destroy log error
#define ELSGMNF -17 // segment not found
#define ELDTSYN -18 // segment sync error
#define ELMTSYN -19 // segment sync error
#define ELEOS   -20 // end of segment
#define ELLOCK  -21 // lock held
#define ELOSLOW -22 // offset is too low, not retained
                    // (producers have been faster than consumers)
#define ELIDXCR -23 // failed to create index
#define ELIDXOP -24 // index operation failed
#define ELWOFFS -25 // failed to find write offset
#define ELLDSGM -26 // failed to load segments
#define ELLCKOP -28 // failed on lock operation
#define ELIDXPC -29 // index operation panic
#define ELIDXLK -30 // index is locked
#define ELIDXNM -31 // key inserted violates monotonicity

#endif
