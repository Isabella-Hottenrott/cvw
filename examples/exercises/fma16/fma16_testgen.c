// fma16_testgen.c
// David_Harris 8 February 2025
// Generate tests for 16-bit FMA
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "softfloat.h"
#include "softfloat_types.h"

typedef union sp {
  float32_t v;
  float f;
} sp;

// lists of tests, terminated with 0x8000
uint16_t easyExponents[] = {15, 0x8000};
uint16_t easyFracts[] = {0, 0x200, 0x8000}; // 1.0 and 1.1



uint16_t medFracts[] = {0, 0x001, 0x3C0, 0x3FF, 0x8000};

// Testing lower, upper exponents, and then 
// more middle range for functionality
uint16_t mulMedExponents[] = {8, 12, 15, 17, 22, 0x8000};
uint16_t addMedExponents[] = {1, 11, 15, 17, 22, 25, 29, 0x8000};
uint16_t fmaMedExponents[] = {1, 17, 21, 0x8000};



void softfloatInit(void) {
    softfloat_roundingMode = softfloat_round_minMag; 
    softfloat_exceptionFlags = 0;
    softfloat_detectTininess = softfloat_tininess_beforeRounding;
}

float convFloat(float16_t f16) {
    float32_t f32;
    float res;
    sp r;

    // convert half to float for printing
    f32 = f16_to_f32(f16);
    r.v = f32;
    res = r.f;
    return res;
}

void genCase(FILE *fptr, float16_t x, float16_t y, float16_t z, int mul, int add, int negp, int negz, int roundingMode, int zeroAllowed, int infAllowed, int nanAllowed) {
    float16_t result;
    int op, flagVals;
    char calc[80], flags[80];
    float32_t x32, y32, z32, r32;
    float xf, yf, zf, rf;
    float16_t x2, z2;
    float16_t smallest;

    if (!mul) y.v = 0x3C00; // force y to 1 to avoid multiply
    if (!add) z.v = 0x0000; // force z to 0 to avoid add
   // if (negp) x.v ^= 0x8000; // flip sign of x to negate p
   // if (negz) z.v ^= 0x8000; // flip sign of z to negate z

    x2 = x;
    z2 = z;

    if (negp) x2.v ^= 0x8000; // flip sign of x to negate p
    if (negz) z2.v ^= 0x8000; // flip sign of z to negate z

    op = roundingMode << 4 | mul<<3 | add<<2 | negp<<1 | negz;
//    printf("op = %02x rm %d mul %d add %d negp %d negz %d\n", op, roundingMode, mul, add, negp, negz);
    softfloat_exceptionFlags = 0; // clear exceptions
   // result = f16_mulAdd(x, y, z); // call SoftFloat to compute expected result
    result = f16_mulAdd(x2, y, z2); // call SoftFloat to compute expected result

    // Extract expected flags from SoftFloat
    sprintf(flags, "NV: %d OF: %d UF: %d NX: %d", 
        (softfloat_exceptionFlags >> 4) % 2,
        (softfloat_exceptionFlags >> 2) % 2,
        (softfloat_exceptionFlags >> 1) % 2,
        (softfloat_exceptionFlags) % 2);
    // pack these four flags into one nibble, discarding DZ flag
    flagVals = softfloat_exceptionFlags & 0x7 | ((softfloat_exceptionFlags >> 1) & 0x8);

    // convert to floats for printing
    xf = convFloat(x);
    yf = convFloat(y);
    zf = convFloat(z);
    rf = convFloat(result);
    if (mul)
        if (add) sprintf(calc, "%f * %f + %f = %f", xf, yf, zf, rf);
        else     sprintf(calc, "%f * %f = %f", xf, yf, rf);
    else         sprintf(calc, "%f + %f = %f", xf, zf, rf);

    // omit denorms, which aren't required for this project
    smallest.v = 0x0400;
    float16_t resultmag = result;
    resultmag.v &= 0x7FFF; // take absolute value
    if (f16_lt(resultmag, smallest) && (resultmag.v != 0x0000)) fprintf (fptr, "// skip denorm: ");
    if ((softfloat_exceptionFlags >> 1) % 2) fprintf(fptr, "// skip underflow: ");

    // skip special cases if requested
    if (resultmag.v == 0x0000 && !zeroAllowed) fprintf(fptr, "// skip zero: ");
    if ((resultmag.v == 0x7C00 || resultmag.v == 0x7BFF) && !infAllowed)  fprintf(fptr, "// Skip inf: ");
    if (resultmag.v >  0x7C00 && !nanAllowed)  fprintf(fptr, "// Skip NaN: ");

    // print the test case
    fprintf(fptr, "%04x_%04x_%04x_%02x_%04x_%01x // %s %s\n", x.v, y.v, z.v, op, result.v, flagVals, calc, flags);
}

void prepTests(uint16_t *e, uint16_t *f, char *testName, char *desc, float16_t *cases, 
               FILE *fptr, int *numCases) {
    int i, j;

    // Loop over all of the exponents and fractions, generating and counting all cases
    fprintf(fptr, "%s", desc); fprintf(fptr, "\n");
    *numCases=0;
    for (i=0; e[i] != 0x8000; i++)
        for (j=0; f[j] != 0x8000; j++) {
            cases[*numCases].v = f[j] | e[i]<<10;
            *numCases = *numCases + 1;
        }
}



void genMulTests(uint16_t *e, uint16_t *f, int sgn, char *testName, char *desc, int roundingMode, int zeroAllowed, int infAllowed, int nanAllowed) {
    int i, j, k, numCases;
    float16_t x, y, z;
    float16_t cases[100000];
    FILE *fptr;
    char fn[80];
 
    sprintf(fn, "work/%s.tv", testName);
    if ((fptr = fopen(fn, "w")) == 0) {
        printf("Error opening to write file %s.  Does directory exist?\n", fn);
        exit(1);
    }
        prepTests(e, f, testName, desc, cases, fptr, &numCases);
        printf("Generated %d test cases for %s:\n", numCases, testName);
        for (int d = 0; d < numCases; d++) {
            printf("Case[%d]: %04x\n", d, cases[d].v);
        }
        z.v = 0x0000;
        for (i=0; i < numCases; i++) { 
            x.v = cases[i].v;
            for (j=0; j< numCases; j++) {
                y.v = cases[j].v;
                for (k=0; k<=sgn; k++) {
                    y.v ^= (k<<15);
                    genCase(fptr, x, y, z, 1, 0, 0, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed);         
                }
            }
        }
    fclose(fptr);
}


void genAddTests(uint16_t *e, uint16_t *f, int sgn, char *testName, char *desc, int roundingMode, int zeroAllowed, int infAllowed, int nanAllowed, int complexity) {
    int i, j, k, numCases, jstart;
    float16_t x, y, z;
    float16_t cases[100000];
    FILE *fptr;
    char fn[80];
 
    sprintf(fn, "work/%s.tv", testName);
    if ((fptr = fopen(fn, "w")) == 0) {
        printf("Error opening to write file %s.  Does directory exist?\n", fn);
        exit(1);
    }
        if (complexity == 0){
            prepTests(e, f, testName, desc, cases, fptr, &numCases);
            y.v = 0x3C00;
            for (i=0; i < numCases; i++) { 
                x.v = cases[i].v;
                for (j=0; j< numCases; j++) {
                    z.v = cases[j].v;
                    for (k=0; k<=sgn; k++) {
                        z.v ^= (k<<15);
                        genCase(fptr, x, y, z, 0, 1, 0, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed);         
                    }
                }
            }
        }

        if (complexity == 1) {
            prepTests(e, f, testName, desc, cases, fptr, &numCases);
            y.v = 0x3C00;
            for (i=0; i < numCases; i++) { 
                x.v = cases[i].v;

                for (j=0; j<numCases; j++) {
                    if ((i == j))  continue;
                    // skips cases where numbers are subtracted 
                    // from themselves and produce 0


                    if ((i <=3) && (j <=3)) continue;
                    // skips cases where numbers are too small
                    // and produce denorms

                    else z.v = cases[j].v;
                    
                    for (k=0; k<=sgn; k++) {
                        z.v ^= (k<<15);
                        genCase(fptr, x, y, z, 0, 1, 0, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed);
                    }
                }
            }
        }
    fclose(fptr);
}




//X * Y + Z
void genMulAddTests(uint16_t *e, uint16_t *f, int sgn, char *testName, char *desc, int roundingMode, int zeroAllowed, int infAllowed, int nanAllowed) {
    int i, j, k, numCases, n;
    float16_t x, y, z;
    float16_t cases[100000];
    FILE *fptr;
    char fn[80];
 
    sprintf(fn, "work/%s.tv", testName);
    if ((fptr = fopen(fn, "w")) == 0) {
        printf("Error opening to write file %s.  Does directory exist?\n", fn);
        exit(1);
    }
    prepTests(e, f, testName, desc, cases, fptr, &numCases);
        for (i=0; i < numCases; i++) { 
            x.v = cases[i].v;
            for (j=0; j<= numCases; j++) {
                if (i == j) continue;
                y.v = cases[j].v;
                for (n=0; n<numCases; n++) {
                    if (n == j) continue;
                    if (n == i) continue;
                    z.v = cases[n].v;
                    for (k=0; k<=sgn; k++) {
                        z.v ^= (k<<15);
                        y.v ^= (k<<15);
                        genCase(fptr, x, y, z, 1, 1, 0, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed); 
                    }
                }
            }
        }
    fclose(fptr);
}

    

// 
void genSpecialcase(int sgn, char *testName, char *desc, int roundingMode, int zeroAllowed, int infAllowed, int nanAllowed) {
    int i, j, k, numCases, n;
    float16_t x, y, z;
    float16_t cases[100000];
    FILE *fptr;
    char fn[80];

    sprintf(fn, "work/%s.tv", testName);
    if ((fptr = fopen(fn, "w")) == 0) {
        printf("Error opening to write file %s.  Does directory exist?\n", fn);
        exit(1);
    }

    fprintf(fptr, "%s", desc); fprintf(fptr, "\n");

    numCases=20;

// Special Cases : 0s
    cases[0].v = 0x0000;
    cases[1].v = 0x8000;

// Special Cases : Infs
    cases[2].v = 0x7C00;
    cases[3].v = 0xFC00;

// Special Cases : NaNs
    cases[4].v = 0xFFFF;
    cases[5].v = 0xFC01;
    cases[6].v = 0x7C01;
    cases[7].v = 0x7FFF;
    cases[8].v = 0x7E00;

// Other : 
    // smallest and biggest 
    cases[10].v = 0x7BFF;
    cases[11].v = 0x0001;

    //tests for underflow
    cases[13].v = 0x07FF;
    cases[13].v = 0x3400;
    cases[13].v = 0x0200;

    // Will give different rounding results
    cases[9].v = 0x3C00;
    cases[10].v = 0x53FF;
    cases[11].v = 0x4006;

    // massive cancellation
    cases[12].v = 0x5200;
    cases[13].v = 0x3500;
    cases[14].v = 0x4A80;

    //additional
    cases[16].v = 0x3E00;
    cases[17].v = 0x3C00;


    // tests for overflow
    cases[18].v = 0x7800;
    cases[19].v = 0x7A00;

    

    for (i=0; i < numCases; i++) { 
        x.v = cases[i].v;
        for (j=0; j< numCases; j++) {
            y.v = cases[j].v;
            for (n=0; n<numCases; n++) {
                z.v = cases[n].v;
                for (k=0; k<=sgn; k++) {
                    z.v ^= (k<<15);
                    for (k=0; k<=sgn; k++) {
                        x.v ^= (k<<15);
                        for (k=0; k<=sgn; k++) {
                            z.v ^= (k<<15);
                            y.v ^= (k<<15);
                            genCase(fptr, x, y, z, 1, 1, 0, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed); 
                        }
                    }
                }
            }
        }
    }

    // testing all possible signage using negp
    for (i=9; i < (numCases-1); i++) { 
            x.v = cases[i].v;
            for (j=12; j< (numCases-1); j++) {
                y.v = cases[j].v;
                for (n=9; n<(numCases-1); n++) {
                    z.v = cases[n].v;
                    for (k=0; k<=sgn; k++) {
                        z.v ^= (k<<15);
                        for (k=0; k<=sgn; k++) {
                            x.v ^= (k<<15);
                            for (k=0; k<=sgn; k++) {
                                z.v ^= (k<<15);
                                genCase(fptr, x, y, z, 1, 1, 1, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed); 
                                y.v ^= (k<<15);
                                genCase(fptr, x, y, z, 1, 1, 1, 0, roundingMode, zeroAllowed, infAllowed, nanAllowed); 
                            }
                        }
                    }
                }
            }
        }
fclose(fptr);
}


int main()
{
    if (system("mkdir -p work") != 0) exit(1); // create work directory if it doesn't exist
    softfloatInit(); // configure softfloat modes
 
    // General Tests with no complexity:
    genMulTests(easyExponents, easyFracts, 0, "fmul_0", "// Multiply with exponent of 0, significand of 1.0 and 1.1, RZ", 0, 0, 0, 0);
    genAddTests(easyExponents, easyFracts, 0, "fadd_0", "// Add with exponent of 0, significand of 1.0 and 1.1, RZ", 0, 0, 0, 0, 0);
    genMulAddTests(easyExponents, easyFracts, 0, "fma_0", "// MulAdd with exponent of 0, significand of 1.0 and 1.1, RZ", 0, 0, 0, 0);



    //General Tests with complexity:
    genMulTests(mulMedExponents, medFracts, 0, "fmul_1", "// Multiply", 0, 0, 0, 0); 
    //Multiplication of positive numbers
    genMulTests(mulMedExponents, medFracts, 1, "fmul_2", "// Multiply", 0, 0, 0, 0);
    //Multiplication with negative numbers, implemented using sgn = 1

    genAddTests(addMedExponents, medFracts, 0, "fadd_1", "// Add", 0, 0, 0, 0, 1);
    //Addition of positive numbers
    genAddTests(addMedExponents, medFracts, 1, "fadd_2", "// Add", 0, 0, 0, 0, 1);
    //Addition with negative numbers, implemented using sgn = 1

    genMulAddTests(fmaMedExponents, medFracts, 0, "fma_1", "// MulAdd", 0, 0, 0, 0);
    // Multiply Add with positive numbers
    genMulAddTests(fmaMedExponents, medFracts, 1, "fma_2", "// MulAdd", 0, 0, 0, 0);
    // Multiply Add with negative numbers, implemented using sgn = 1




    //Complex Tests testing signage and rounding modes:

    //_rz tests:
    genSpecialcase(1, "fmaSpecial_rz", "// MulAdd", 0, 1, 1, 1);
    // Multiply Add testing all possible signage, 0, inf nan allowed, RZ

    // _rne tests:
    softfloat_roundingMode = softfloat_round_near_even; 
    genSpecialcase(1, "fmaSpecial_rne", "// MulAdd", 1, 1, 1, 1);
    // Multiply Add testing all possible signage, 0, inf nan allowed, Rne

    // _rp tests:
    softfloat_roundingMode = softfloat_round_max;
    genSpecialcase(1, "fmaSpecial_rp", "// MulAdd", 3, 1, 1, 1);
    // Multiply Add testing all possible signage, 0, inf nan allowed, Rp

    // _rm tests:
    softfloat_roundingMode = softfloat_round_min;
    genSpecialcase(1, "fmaSpecial_rm", "// MulAdd", 2, 1, 1, 1);
    // Multiply Add testing all possible signage, 0, inf nan allowed, Rm


    return 0;
}
