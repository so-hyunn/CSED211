/* 20210741 김소현 */
/* sooohyun@postech.ac.kr */

/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int tmp;
    int s0, s1, s2, s3, s4, s5, s6, s7;

    if(M == 32 && N == 32) {
        for (s0 = 0; 
            s0 < N; 
            s0 += 8) 
            {
            for (s1 = 0; 
                s1 < M; 
                s1 += 8) 
                {
                for (i = s0; i < s0 + 8; i++) 
                {
                    for (j = s1; j < s1 + 8; j++) 
                    {
                        if (i != j)
                        {
                           B[j][i] = A[i][j];
                        }
                        else tmp = A[i][i];
                    }
                    if (s1 == s0)
                        B[i][i] = tmp;
                }
            }
        }
    }

    else if( M == 64 && N == 64) 
    {
        for (i = 0; i < 64; i += 8) {
            for (j = 0; j < 64; j += 8) {

                for (k = i ; k < i+4 ; k ++)
                    {
                        s0 = A[k][j];
                        s1 = A[k][j+1];
                        s2 = A[k][j+2];
                        s3 = A[k][j+3];
                        s4 = A[k][j+4];
                        s5 = A[k][j+5];
                        s6 = A[k][j+6];
                        s7 = A[k][j+7];

                        B[j][k] = s0;
                        B[j+1][k] = s1;
                        B[j+2][k] = s2;
                        B[j+3][k] = s3;

                        B[j][k+4] = s7;
                        B[j+1][k+4] = s6;
                        B[j+2][k+4] = s5;
                        B[j+3][k+4] = s4;
                    }


                    for (k=0;k<4;k++)
                    {
                        s0 = A[i+4][j+3-k];
                        s1 = A[i+5][j+3-k];
                        s2 = A[i+6][j+3-k];
                        s3 = A[i+7][j+3-k];

                        s4 = A[i+4][j+4+k];
                        s5 = A[i+5][j+4+k];
                        s6 = A[i+6][j+4+k];
                        s7 = A[i+7][j+4+k];

                        B[j+k+4][i] = B[j+3-k][i+4];
                        B[j+k+4][i+1] = B[j+3-k][i+5];
                        B[j+k+4][i+2] = B[j+3-k][i+6];
                        B[j+k+4][i+3] = B[j+3-k][i+7];

                        B[j+3-k][i+4] = s0;
                        B[j+3-k][i+5] = s1;
                        B[j+3-k][i+6] = s2;
                        B[j+3-k][i+7] = s3;

                        B[j+4+k][i+4] = s4;
                        B[j+4+k][i+5] = s5;
                        B[j+4+k][i+6] = s6;
                        B[j+4+k][i+7] = s7;
                    }
            }
        }
        }
    
    
    else if(M==61 && N==67) 
    {
        int tmp;
        for (s0 = 0; 
            s0 < N; 
           s0 += 16) 
            {
            for (s1 = 0; s1 < M; 
                s1 += 16) 
                {
                for (i = s0; i < N && i < s0 + 16; i++) 
                {
                    for (j = s1; j < M && j < s1 + 16; j++) 
                    {
                        if (i != j)
                           B[j][i] = A[i][j];
                        else tmp = A[i][i];
                    }
                    if (s1== s0)
                        B[i][i] = tmp;
                }
            }
        }
    }
    else {
        printf("Out of Case");
        return;
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

