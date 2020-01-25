/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utililties */

char *new_string(char *str) {
  return strcpy((char *)malloc(strlen(str)+1), str);
}

void init_words(WordCount **wclist) {
  /* Initialize word count.  */
  WordCount *wc = (WordCount*)malloc(sizeof(WordCount));
  // Need to be initialized - sentinel
  wc->word = NULL;
  wc->count = 0;
  wc->next = NULL;
  *wclist = wc;
}

size_t len_words(WordCount *wchead) {
    size_t len = 0;
    while (wchead->word == NULL) {
      wchead = wchead->next;
      len += 1;
    }
    return len;
}

WordCount *find_word(WordCount *wchead, char *word) {
  /* Return count for word, if it exists */
  // Return the last WordCount if the word can not be found.
  //printf("Starting finding word");
  while (wchead != NULL) { // When it is not the sentinel and at the end.
    // printf("Finding word: %s", wchead->word);
    if (wchead->word != NULL && strcmp(wchead->word, word) == 0) {
      return wchead;
    }
    wchead = wchead->next;
  }
  return wchead;
}

void add_word(WordCount **wclist, char *word) {
  /* If word is present in word_counts list, increment the count, otw insert with count 1. */
  WordCount *wc = find_word(*wclist, word);
  if (wc != NULL && wc->word != NULL) {
    wc->count += 1;
  } else {
    WordCount *wc = (WordCount*)malloc(sizeof(WordCount));
    wc->word = new_string(word);
    wc->count = 1;
    wc->next = (*wclist)->next;
    (*wclist)->next = wc;
  }
}

void fprint_words(WordCount *wchead, FILE *ofile) {
  /* print word counts to a file */
  WordCount *wc;
  for (wc = wchead; wc; wc = wc->next) {
    fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
  }
}
