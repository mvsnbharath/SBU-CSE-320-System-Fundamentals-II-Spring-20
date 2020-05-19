/* Structure of the prologue. */
typedef struct sf_prologue {
    sf_header header;
    sf_footer padding1;
    sf_footer padding2;
    sf_footer padding3;
    sf_footer padding4;
    sf_footer padding5;
    sf_footer padding6;
    sf_footer footer;
} sf_prologue;

/* Structure of the epilogue. */
typedef struct sf_epilogue {
    sf_header header;
} sf_epilogue;