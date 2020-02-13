/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2019 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>

#include "word_count.h"
#include "word_helpers.h"

/*
struct arg_struct {
  word_count_list_t *lst;
  char *filename;
};
*/

word_count_list_t *shared_lst;

void *process_file(void *filename) {
  // printf("File %s is processing", (char*) filename);
  //struct arg_struct *args = (struct arg_struct *) arguments;
  FILE *infile = fopen((char *) filename, "r");
  if (infile == NULL) {
    perror("fopen");
  }
  count_words(shared_lst, infile);
  fclose(infile);
  pthread_exit(NULL);
}

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char *argv[]) {
  // printf("Are you here");
  /* Create the empty data structure. */
  word_count_list_t word_counts;
  init_words(&word_counts);

  if (pthread_mutex_init(&(word_counts.lock), NULL) != 0) {
    printf("\n Mutex init has failed\n"); 
    return -1; 
  }

  if (argc <= 1) {
    /* Process stdin in a single thread. */
    // Whatsup
    count_words(&word_counts, stdin);
  } else {
    /* TODO */
    int nthreads = argc - 1;
    int rc;
    pthread_t threads[nthreads];
    long t;
    shared_lst = &word_counts;
    for (t = 1; t <= nthreads; t++) {
      rc = pthread_create(&threads[t-1], NULL, process_file, (void *) argv[t]);
      if (rc) {
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
      }
    }
    for (t = 1; t <= nthreads; t++)
      pthread_join(threads[t-1], NULL);
  }

  /* Output final result of all threads' work. */
  wordcount_sort(&word_counts, less_count);
  fprint_words(&word_counts, stdout);
  exit(0);
}
