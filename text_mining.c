/*
 * Dynamic Text Mining and Linguistic Pattern Analyzer
 *
 * Source text: a paragraph (150+ words) from "Purple Hibiscus" by
 * Chimamanda Ngozi Adichie, pasted in by the user at run time.
 *
 * TEXT-PROCESSING RULES
 *   Case        - every ASCII letter is converted to lowercase, so "Papa" and
 *                 "papa" are the same word.
 *   Punctuation - any character that is not a letter or digit separates words
 *                 (spaces, tabs, newlines, . , ; : ! ? " ( ) and so on).
 *                 Repeated whitespace and punctuation is skipped as one gap.
 *   Apostrophes - kept only when they sit between two word characters, so
 *                 "didn't" and "Jaja's" stay one word. Leading or trailing
 *                 apostrophes (quotation marks) are dropped. The curly
 *                 apostrophe (U+2019) is stored as a plain ' so both spellings
 *                 count as the same word.
 *   Hyphens     - kept only between two word characters ("well-known" is one
 *                 word). Double hyphens, en dashes and em dashes separate words.
 *   Accents     - non-ASCII letters such as the e-acute in "etagere" are kept
 *                 as part of the word; word length is measured in characters,
 *                 not bytes.
 *   Long words  - only the first 39 bytes of a word fit in wordStat.name. Longer
 *                 words are truncated at a character boundary, counted as
 *                 truncated, and their true length is kept in wordStat.length.
 *                 Two long words that share the same first 39 bytes are
 *                 counted as one word.
 *
 * CUSTOM ANALYSIS: Lexical Richness Profile
 *   N  = total words (tokens), V = distinct words (types),
 *   V1 = hapax legomena (words that occur exactly once)
 *   Type-token ratio      TTR = V / N
 *   Hapax ratio               = V1 / V
 *   Repetition intensity      = (N - V1) / N   (share of tokens that repeat)
 *   Honore's R                = 100 * ln(N) / (1 - V1 / V)
 *   TTR falls as texts get longer, so Honore's R is included because it is less
 *   sensitive to length. A higher R means a richer, less repetitive vocabulary.
 *
 * Build: gcc -Wall -Wextra -o text_mining text_mining.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Details printed in the output header */
#define STUDENT_ID "XXXXX"
#define PROJECT_NAME "Dynamic Text Mining and Linguistic Pattern Analyzer"
#define SOURCE_TITLE "Purple Hibiscus"
#define SOURCE_AUTHOR "Chimamanda Ngozi Adichie"
#define SOURCE_PUBLISHER "Algonquin Books of Chapel Hill, 2003"
#define SOURCE_SECTION "Breaking Gods: Palm Sunday"
#define CUSTOM_ANALYSIS "Lexical Richness Profile"

#define MAX_WORD_LEN 39
#define MIN_WORDS_REQUIRED 150

/* Deliberately small starting sizes so the buffers visibly grow with realloc */
#define TEXT_INITIAL_CAP 64
#define WORDS_INITIAL_CAP 8

struct wordStat
{
    char name[MAX_WORD_LEN + 1];
    int count;
    int length; /* true length in characters, even when name was truncated */
};

typedef struct
{
    char *text; /* dynamically allocated, NUL terminated paragraph */
    size_t text_len;
    size_t text_cap;

    struct wordStat *words; /* dynamically allocated array of distinct words */
    int distinct;
    int word_cap;

    int total_words;
    int truncated_words;
    const char *order; /* describes the current order of the words array */
} TextAnalysis;

/* Every menu operation has this shape so they can live in one dispatch table */
typedef void (*analysis_fn)(TextAnalysis *t);
typedef int (*compare_fn)(const void *a, const void *b);

struct menu_item
{
    const char *label;
    analysis_fn run;
    int needs_text;
};

/* ---------------------------------------------------------------------------
 * Input helpers
 * ------------------------------------------------------------------------- */

/* Reads one line into buf, dropping the newline and anything that did not fit */
int read_line(char *buf, size_t size)
{
    size_t len;
    int ch;

    if (fgets(buf, (int)size, stdin) == NULL)
        return 0;

    len = strlen(buf);

    if (len > 0 && buf[len - 1] == '\n')
        buf[--len] = '\0';
    else
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;

    if (len > 0 && buf[len - 1] == '\r')
        buf[--len] = '\0';

    return 1;
}

/* True when the line [line, line + len) holds only the word END */
int is_end_marker(const char *line, size_t len)
{
    while (len > 0 && isspace((unsigned char)line[len - 1]))
        len--;

    return len == 3 && strncmp(line, "END", 3) == 0;
}

/* ---------------------------------------------------------------------------
 * Pointer-based word extraction
 * ------------------------------------------------------------------------- */

/* Number of bytes in the UTF-8 character starting at p (1 for ASCII or bad bytes) */
int utf8_char_len(const unsigned char *p)
{
    int len, i;

    if (*p < 0xC0)
        return 1;
    else if (*p < 0xE0)
        len = 2;
    else if (*p < 0xF0)
        len = 3;
    else if (*p < 0xF8)
        len = 4;
    else
        return 1;

    /* Stops at the NUL terminator, so it never reads past the buffer */
    for (i = 1; i < len; i++)
        if ((p[i] & 0xC0) != 0x80)
            return 1;

    return len;
}

/* Bytes of whitespace or punctuation at p, or 0 when p starts a word character */
int separator_len(const unsigned char *p)
{
    if (*p < 0x80)
        return isalnum(*p) ? 0 : 1;

    /* U+2000 block: curly quotes, en and em dashes, ellipsis */
    if (p[0] == 0xE2 && p[1] == 0x80 && (p[2] & 0xC0) == 0x80)
        return 3;

    /* U+0080 to U+00BF: non-breaking space, guillemets, inverted ! and ? */
    if (p[0] == 0xC2 && (p[1] & 0xC0) == 0x80)
        return 2;

    return 0;
}

/* Bytes of an apostrophe or hyphen at p, storing the character to keep in *as */
int joiner_len(const unsigned char *p, char *as)
{
    if (*p == '\'' || *p == '-')
    {
        *as = (char)*p;
        return 1;
    }

    /* Right single quotation mark, the usual curly apostrophe */
    if (p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0x99)
    {
        *as = '\'';
        return 3;
    }

    return 0;
}

/*
 * Extracts the next word at *cursor into out (lowercased, at most MAX_WORD_LEN
 * bytes) and moves *cursor past it. Returns the word's length in characters,
 * or 0 when the text has no more words.
 */
int next_word(const char **cursor, char *out, int *truncated)
{
    const unsigned char *p = (const unsigned char *)*cursor;
    char *dst = out;
    char *end = out + MAX_WORD_LEN;
    int length = 0;
    int full = 0;
    int n;
    char joiner;

    while (*p != '\0' && separator_len(p) > 0)
        p += separator_len(p);

    while (*p != '\0')
    {
        if (separator_len(p) == 0)
        {
            n = utf8_char_len(p);

            /* Only copy whole characters so a cut never splits a UTF-8 sequence */
            if (!full && dst + n <= end)
            {
                if (n == 1)
                    *dst++ = (char)tolower(*p);
                else
                {
                    memcpy(dst, p, n);
                    dst += n;
                }
            }
            else
                full = 1;

            p += n;
            length++;
        }
        else if ((n = joiner_len(p, &joiner)) > 0 && p[n] != '\0' && separator_len(p + n) == 0)
        {
            if (!full && dst < end)
                *dst++ = joiner;
            else
                full = 1;

            p += n;
            length++;
        }
        else
            break;
    }

    *dst = '\0';
    *cursor = (const char *)p;

    if (truncated != NULL)
        *truncated = full;

    return length;
}

/* ---------------------------------------------------------------------------
 * Dynamic storage
 * ------------------------------------------------------------------------- */

void free_analysis(TextAnalysis *t)
{
    free(t->text);
    free(t->words);
    t->text = NULL;
    t->words = NULL;
    t->text_len = t->text_cap = 0;
    t->distinct = t->word_cap = 0;
    t->total_words = t->truncated_words = 0;
}

/* Doubles the text buffer until it can hold needed bytes */
int ensure_text_capacity(TextAnalysis *t, size_t needed)
{
    size_t new_cap = t->text_cap;
    char *grown;

    if (needed <= t->text_cap)
        return 1;

    while (new_cap < needed)
        new_cap *= 2;

    grown = realloc(t->text, new_cap);

    if (grown == NULL)
    {
        printf("Memory allocation failed while growing the text buffer.\n");
        return 0;
    }

    printf("  [memory] text buffer grown %lu -> %lu bytes, now at %p\n",
           (unsigned long)t->text_cap, (unsigned long)new_cap, (void *)grown);

    t->text = grown;
    t->text_cap = new_cap;
    return 1;
}

/* Reads characters until a line holding only END, or end of input */
int read_text(TextAnalysis *t)
{
    size_t line_start = 0;
    int ch;

    t->text = malloc(TEXT_INITIAL_CAP);

    if (t->text == NULL)
    {
        printf("Memory allocation failed for the text buffer.\n");
        return 0;
    }

    t->text_cap = TEXT_INITIAL_CAP;
    t->text_len = 0;
    printf("  [memory] text buffer allocated: %d bytes at %p\n", TEXT_INITIAL_CAP, (void *)t->text);

    while ((ch = getchar()) != EOF)
    {
        if (ch == '\n')
        {
            if (is_end_marker(t->text + line_start, t->text_len - line_start))
            {
                t->text_len = line_start;
                break;
            }
        }

        /* Room for this character plus the terminator */
        if (!ensure_text_capacity(t, t->text_len + 2))
            return 0;

        t->text[t->text_len++] = (char)ch;

        if (ch == '\n')
            line_start = t->text_len;
    }

    /* END typed on the last line without a newline */
    if (ch == EOF && is_end_marker(t->text + line_start, t->text_len - line_start))
        t->text_len = line_start;

    t->text[t->text_len] = '\0';
    return 1;
}

struct wordStat *find_word(const TextAnalysis *t, const char *word)
{
    struct wordStat *entry;

    for (entry = t->words; entry < t->words + t->distinct; entry++)
        if (strcmp(entry->name, word) == 0)
            return entry;

    return NULL;
}

/* Appends a new distinct word, doubling the array when it is full */
int add_word(TextAnalysis *t, const char *word, int length)
{
    struct wordStat *grown;
    struct wordStat *entry;
    int new_cap;

    if (t->distinct == t->word_cap)
    {
        new_cap = t->word_cap * 2;
        grown = realloc(t->words, new_cap * sizeof *grown);

        if (grown == NULL)
        {
            printf("Memory allocation failed while growing the word statistics.\n");
            return 0;
        }

        printf("  [memory] word statistics grown %d -> %d entries, now at %p\n",
               t->word_cap, new_cap, (void *)grown);

        t->words = grown;
        t->word_cap = new_cap;
    }

    entry = t->words + t->distinct;

    /* next_word never writes more than MAX_WORD_LEN bytes, but check before copying */
    if (strlen(word) >= sizeof entry->name)
        return 0;

    strcpy(entry->name, word);
    entry->count = 1;
    entry->length = length;
    t->distinct++;
    return 1;
}

int build_statistics(TextAnalysis *t)
{
    const char *cursor = t->text;
    char word[MAX_WORD_LEN + 1];
    struct wordStat *entry;
    int length, truncated;

    t->words = calloc(WORDS_INITIAL_CAP, sizeof *t->words);

    if (t->words == NULL)
    {
        printf("Memory allocation failed for the word statistics.\n");
        return 0;
    }

    t->word_cap = WORDS_INITIAL_CAP;
    t->distinct = 0;
    t->total_words = 0;
    t->truncated_words = 0;
    t->order = "order of first appearance";
    printf("  [memory] word statistics allocated: %d entries at %p\n", WORDS_INITIAL_CAP, (void *)t->words);

    while ((length = next_word(&cursor, word, &truncated)) > 0)
    {
        t->total_words++;

        if (truncated)
            t->truncated_words++;

        entry = find_word(t, word);

        if (entry != NULL)
            entry->count++;
        else if (!add_word(t, word, length))
            return 0;
    }

    return 1;
}

/* Reads a paragraph and builds its statistics. Returns 0 on allocation failure */
int load_text(TextAnalysis *t)
{
    free_analysis(t);

    printf("\nPaste or type a paragraph of at least %d words.\n", MIN_WORDS_REQUIRED);
    printf("Type END on its own line when finished.\n\n");

    if (!read_text(t) || !build_statistics(t))
    {
        free_analysis(t);
        return 0;
    }

    if (t->total_words == 0)
        printf("\nNo words were found in the input.\n");
    else
    {
        printf("\nRead %lu characters: %d words, %d distinct.\n",
               (unsigned long)t->text_len, t->total_words, t->distinct);

        if (t->total_words < MIN_WORDS_REQUIRED)
            printf("Note: the paragraph has fewer than %d words.\n", MIN_WORDS_REQUIRED);

        if (t->truncated_words > 0)
            printf("Note: %d word(s) were longer than %d bytes and were truncated.\n",
                   t->truncated_words, MAX_WORD_LEN);
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 * Shared calculations
 * ------------------------------------------------------------------------- */

/* Longest word; ties go to the alphabetically first so the answer never depends on sorting */
struct wordStat *find_longest(const TextAnalysis *t)
{
    struct wordStat *best = t->words;
    struct wordStat *entry;

    for (entry = t->words + 1; entry < t->words + t->distinct; entry++)
        if (entry->length > best->length ||
            (entry->length == best->length && strcmp(entry->name, best->name) < 0))
            best = entry;

    return best;
}

struct wordStat *find_most_frequent(const TextAnalysis *t)
{
    struct wordStat *best = t->words;
    struct wordStat *entry;

    for (entry = t->words + 1; entry < t->words + t->distinct; entry++)
        if (entry->count > best->count ||
            (entry->count == best->count && strcmp(entry->name, best->name) < 0))
            best = entry;

    return best;
}

double average_word_length(const TextAnalysis *t)
{
    const struct wordStat *entry;
    long characters = 0;

    for (entry = t->words; entry < t->words + t->distinct; entry++)
        characters += (long)entry->length * entry->count;

    return (double)characters / t->total_words;
}

int count_hapax(const TextAnalysis *t)
{
    const struct wordStat *entry;
    int hapax = 0;

    for (entry = t->words; entry < t->words + t->distinct; entry++)
        if (entry->count == 1)
            hapax++;

    return hapax;
}

/* Lists every other word whose field (length or count) matches the winner */
void print_ties(const TextAnalysis *t, const struct wordStat *winner, int by_length)
{
    const struct wordStat *entry;
    int ties = 0;

    for (entry = t->words; entry < t->words + t->distinct; entry++)
    {
        if (entry == winner)
            continue;

        if ((by_length && entry->length == winner->length) ||
            (!by_length && entry->count == winner->count))
        {
            printf("%s%s", ties == 0 ? "Tied with: " : ", ", entry->name);
            ties++;
        }
    }

    if (ties > 0)
        printf("\n");
}

/* ---------------------------------------------------------------------------
 * Menu operations
 * ------------------------------------------------------------------------- */

/* Walks the text buffer again with pointers rather than trusting the stored total */
void count_words(TextAnalysis *t)
{
    const char *cursor = t->text;
    char word[MAX_WORD_LEN + 1];
    int words = 0;

    while (next_word(&cursor, word, NULL) > 0)
        words++;

    printf("\nWORD COUNT\n");
    printf("Total words: %d\n", words);
    printf("Characters in the text buffer: %lu\n", (unsigned long)t->text_len);
}

void longest_word(TextAnalysis *t)
{
    struct wordStat *longest = find_longest(t);

    printf("\nLONGEST WORD\n");
    printf("Word: %s\n", longest->name);
    printf("Length: %d characters\n", longest->length);

    if ((size_t)longest->length > strlen(longest->name))
        printf("(The stored name is truncated to %d bytes.)\n", MAX_WORD_LEN);

    print_ties(t, longest, 1);
}

void most_frequent(TextAnalysis *t)
{
    struct wordStat *top = find_most_frequent(t);

    printf("\nMOST FREQUENT WORD\n");
    printf("Word: %s\n", top->name);
    printf("Frequency: %d of %d words (%.2f%%)\n", top->count, t->total_words,
           100.0 * top->count / t->total_words);
    print_ties(t, top, 0);
}

void display_word_statistics(TextAnalysis *t)
{
    const struct wordStat *entry;
    int rank = 1;

    printf("\nWORD STATISTICS (%s)\n", t->order);
    printf("%-5s %-25s %6s %7s %8s\n", "No.", "Word", "Count", "Length", "Share");
    printf("-------------------------------------------------------\n");

    for (entry = t->words; entry < t->words + t->distinct; entry++, rank++)
    {
        /* Pad by characters rather than bytes so accented words stay aligned */
        printf("%-5d %s%*s %6d %7d %7.2f%%\n", rank, entry->name,
               entry->length < 25 ? 25 - entry->length : 0, "", entry->count, entry->length,
               100.0 * entry->count / t->total_words);
    }

    printf("-------------------------------------------------------\n");
    printf("Distinct words: %d   Total words: %d\n", t->distinct, t->total_words);
}

int compare_alphabetical(const void *a, const void *b)
{
    return strcmp(((const struct wordStat *)a)->name, ((const struct wordStat *)b)->name);
}

/* Highest count first, alphabetical within the same count */
int compare_frequency(const void *a, const void *b)
{
    const struct wordStat *x = a;
    const struct wordStat *y = b;

    if (x->count != y->count)
        return y->count - x->count;

    return strcmp(x->name, y->name);
}

void sort_word_statistics(TextAnalysis *t)
{
    static const compare_fn comparators[] = {compare_alphabetical, compare_frequency};
    static const char *orders[] = {"alphabetical order", "frequency order, highest first"};
    char line[32];
    int choice;

    printf("\nSort by:\n1. Alphabetical order\n2. Frequency (highest first)\nChoice: ");

    if (!read_line(line, sizeof line))
        return;

    choice = atoi(line);

    if (choice < 1 || choice > 2)
    {
        printf("Invalid choice. Enter 1 or 2.\n");
        return;
    }

    qsort(t->words, t->distinct, sizeof *t->words, comparators[choice - 1]);
    t->order = orders[choice - 1];
    display_word_statistics(t);
}

void search_word(TextAnalysis *t)
{
    char line[256];
    char word[MAX_WORD_LEN + 1];
    const char *cursor = line;
    struct wordStat *entry;

    printf("\nWord to search for: ");

    if (!read_line(line, sizeof line))
        return;

    /* Normalise the query with the same rules used on the text */
    if (next_word(&cursor, word, NULL) == 0)
    {
        printf("No word was entered.\n");
        return;
    }

    entry = find_word(t, word);

    if (entry == NULL)
        printf("\"%s\" does not appear in the text.\n", word);
    else
        printf("\"%s\" appears %d time(s), %.2f%% of all words.\n", entry->name, entry->count,
               100.0 * entry->count / t->total_words);
}

void lexical_richness_profile(TextAnalysis *t)
{
    int n = t->total_words;
    int v = t->distinct;
    int v1 = count_hapax(t);
    double ttr = (double)v / n;

    printf("\nCUSTOM ANALYSIS: %s\n", CUSTOM_ANALYSIS);
    printf("Tokens (N) = %d, types (V) = %d, words used once (V1) = %d\n\n", n, v, v1);

    printf("Type-token ratio     V / N             = %.3f\n", ttr);
    printf("Hapax ratio          V1 / V            = %.3f\n", (double)v1 / v);
    printf("Repetition intensity (N - V1) / N      = %.3f\n", (double)(n - v1) / n);

    if (v1 == v)
        printf("Honore's R           undefined, every word is used only once\n");
    else
        printf("Honore's R           100 ln N / (1 - V1/V) = %.1f\n", 100.0 * log(n) / (1.0 - (double)v1 / v));

    printf("\nInterpretation: ");

    if (ttr >= 0.70)
        printf("very varied vocabulary; almost every word is new.\n");
    else if (ttr >= 0.50)
        printf("varied vocabulary with some deliberate repetition.\n");
    else if (ttr >= 0.35)
        printf("moderate variety; key words and function words repeat often.\n");
    else
        printf("highly repetitive vocabulary.\n");

    printf("%.1f%% of the words in the text belong to a word that occurs more than once.\n",
           100.0 * (n - v1) / n);
}

void generate_text_summary(TextAnalysis *t)
{
    struct wordStat *longest = find_longest(t);
    struct wordStat *top = find_most_frequent(t);

    printf("\nTEXT SUMMARY\n");
    printf("Source: %s by %s\n", SOURCE_TITLE, SOURCE_AUTHOR);
    printf("------------------------------------------\n");
    printf("Total words:          %d\n", t->total_words);
    printf("Distinct words:       %d\n", t->distinct);
    printf("Average word length:  %.2f characters\n", average_word_length(t));
    printf("Longest word:         %s (%d characters)\n", longest->name, longest->length);
    printf("Most frequent word:   %s (%d times)\n", top->name, top->count);
    printf("Truncated words:      %d\n", t->truncated_words);

    lexical_richness_profile(t);
}

void memory_inspection(TextAnalysis *t)
{
    struct wordStat *selected = find_most_frequent(t);
    struct wordStat *last = t->words + t->distinct - 1;

    printf("\nMEMORY INSPECTION\n");
    printf("------------------------------------------\n");
    printf("Analysis structure:        %p  (local struct in main holding the pointers below)\n", (void *)t);
    printf("Text buffer address:       %p  (heap block of %lu bytes holding the paragraph)\n",
           (void *)t->text, (unsigned long)t->text_cap);
    printf("Word statistics array:     %p  (heap array of %d wordStat entries, %d in use)\n",
           (void *)t->words, t->word_cap, t->distinct);
    printf("First entry  words[0]:     %p  (\"%s\", same address as the array)\n",
           (void *)t->words, t->words->name);
    printf("Selected entry \"%s\":   %p  (most frequent word, index %d)\n",
           selected->name, (void *)selected, (int)(selected - t->words));
    printf("Last entry   words[%d]:   %p  (\"%s\")\n", t->distinct - 1, (void *)last, last->name);
    printf("\nEach wordStat is %lu bytes, so an entry's address is\n", (unsigned long)sizeof *t->words);
    printf("array address + index * %lu. Addresses change between runs and\n", (unsigned long)sizeof *t->words);
    printf("whenever realloc moves a buffer, and sorting moves entries to new indexes.\n");
}

void show_rules(TextAnalysis *t)
{
    (void)t;

    printf("\nTEXT-PROCESSING RULES\n");
    printf("- Letters are converted to lowercase, so \"Papa\" and \"papa\" are one word.\n");
    printf("- Spaces, tabs, newlines and punctuation separate words; repeats count as one gap.\n");
    printf("- An apostrophe between letters is kept (didn't, Jaja's). Quote marks are dropped.\n");
    printf("  Curly apostrophes are stored as ' so both spellings match.\n");
    printf("- A hyphen between letters is kept (well-known). Dashes (--, en, em) separate words.\n");
    printf("- Accented letters stay in the word; lengths are counted in characters.\n");
    printf("- Words longer than %d bytes are truncated; their true length is still recorded.\n", MAX_WORD_LEN);
    printf("\nCUSTOM ANALYSIS FORMULAS\n");
    printf("  N = total words, V = distinct words, V1 = words used exactly once\n");
    printf("  Type-token ratio     = V / N\n");
    printf("  Hapax ratio          = V1 / V\n");
    printf("  Repetition intensity = (N - V1) / N\n");
    printf("  Honore's R           = 100 * ln(N) / (1 - V1 / V)\n");
}

void load_new_paragraph(TextAnalysis *t)
{
    if (!load_text(t))
        printf("The new paragraph could not be stored.\n");
}

/* ---------------------------------------------------------------------------
 * Main program
 * ------------------------------------------------------------------------- */

void print_header(void)
{
    printf("==========================================================\n");
    printf("Student ID: %s\n", STUDENT_ID);
    printf("Project: %s\n", PROJECT_NAME);
    printf("Source: %s, %s\n", SOURCE_TITLE, SOURCE_AUTHOR);
    printf("        %s, \"%s\"\n", SOURCE_PUBLISHER, SOURCE_SECTION);
    printf("Custom Analysis: %s\n", CUSTOM_ANALYSIS);
    printf("==========================================================\n");
}

int main(void)
{
    static const struct menu_item menu[] = {
        {"Count words", count_words, 1},
        {"Longest word", longest_word, 1},
        {"Most frequent word", most_frequent, 1},
        {"Display word statistics", display_word_statistics, 1},
        {"Sort word statistics", sort_word_statistics, 1},
        {"Search for a word", search_word, 1},
        {"Generate text summary", generate_text_summary, 1},
        {"Custom analysis: " CUSTOM_ANALYSIS, lexical_richness_profile, 1},
        {"Memory inspection", memory_inspection, 1},
        {"Show text-processing rules", show_rules, 0},
        {"Analyse a new paragraph", load_new_paragraph, 0},
    };
    const int items = sizeof menu / sizeof menu[0];
    TextAnalysis t = {0};
    analysis_fn run;
    char line[32];
    int choice, i;

#ifdef _WIN32
    /* Lets pasted curly quotes and accented letters arrive as UTF-8 */
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    print_header();

    if (!load_text(&t))
        return 1;

    while (1)
    {
        printf("\n========== MENU ==========\n");

        for (i = 0; i < items; i++)
            printf("%2d. %s\n", i + 1, menu[i].label);

        printf(" 0. Exit\nChoice: ");

        if (!read_line(line, sizeof line))
            break;

        choice = atoi(line);

        if (choice == 0 && strcmp(line, "0") == 0)
            break;

        if (choice < 1 || choice > items)
        {
            printf("Invalid choice. Enter a number between 0 and %d.\n", items);
            continue;
        }

        if (menu[choice - 1].needs_text && t.total_words == 0)
        {
            printf("There is no text to analyse. Choose option %d to enter a paragraph.\n", items);
            continue;
        }

        /* Dispatch through the function pointer stored for this menu entry */
        run = menu[choice - 1].run;
        run(&t);
    }

    free_analysis(&t);
    printf("\nAll memory freed. Goodbye.\n");
    return 0;
}
