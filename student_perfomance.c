#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* strcasecmp is POSIX only, so map it to the MSVC equivalent when needed */
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* Grading rules used by every function, so they only live in one place */
#define PASS_MARK 60
#define CRITICAL_LIMIT 20
#define HIGH_LIMIT 40
#define MODERATE_LIMIT 50

typedef struct Student{
    int id;
    char name[50];
    char course[50];
    float grade;

} students;

students *s;
int capacity, count;

/* Risk tiers, in the same order as the names below */
enum { CRITICAL, HIGH, MODERATE, LOW, PASSED, TIERS };
const char *tier_names[TIERS] = {"CRITICAL", "HIGH", "MODERATE", "LOW", "NO RISK - PASS"};

//struct for each student
int get_number(const char *prompt, void *value, char type)
{
    int result;
    int ch;
    int invalid;

    while (1)
    {
        printf("%s", prompt);

        if (type == 'i')
            result = scanf("%d", (int *)value);
        else if (type == 'f')
            result = scanf("%f", (float *)value);
        else
            return 0;

        if (result == EOF)
            return EOF;

        if (result != 1)
        {
            printf("Invalid input. Try again.\n");

            while ((ch = getchar()) != '\n' && ch != EOF)
                ;

            continue;
        }

        /* Read the rest of the line. Anything other than spaces or tabs makes it invalid */
        invalid = 0;

        while ((ch = getchar()) != '\n' && ch != EOF)
        {
            if (ch != ' ' && ch != '\t')
                invalid = 1;
        }

        if (invalid)
        {
            printf("Invalid input. Try again.\n");
            continue;
        }

        return 1;
    }
}

void get_string(const char *prompt, char *value, size_t size)
{
    int ch;
    int has_letter;
    size_t len;

    while (1)
    {
        printf("%s", prompt);

        if (fgets(value, size, stdin) == NULL)
        {
            free(s);
            exit(0);
        }

        len = strlen(value);

        if (len > 0 && value[len - 1] != '\n')
        {
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;

            printf("Input too long. Try again.\n");
            continue;
        }

        if (len > 0 && value[len - 1] == '\n')
            value[len - 1] = '\0';

        if (value[0] == '\0')
        {
            printf("Input cannot be empty.\n");
            continue;
        }

        has_letter = 0;

        for (int i = 0; value[i] != '\0'; i++)
        {
            if (isalpha((unsigned char)value[i]))
            {
                has_letter = 1;
                break;
            }
        }

        if (!has_letter)
        {
            printf("Input must contain at least one letter.\n");
            continue;
        }

        return;
    }
}

/* A valid name or course has at least two letters and only letters, spaces, - ' . (plus digits for a course) */
int valid_text(const char *value, int allow_digits)
{
    int letters = 0;

    for (int i = 0; value[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)value[i];

        if (isalpha(c))
            letters++;
        else if (c == ' ' || c == '-' || c == '\'' || c == '.')
            continue;
        else if (allow_digits && isdigit(c))
            continue;
        else
            return 0;
    }

    return letters >= 2;
}

/* Same as get_string, but keeps asking until the text looks like a real name or course */
void get_name(const char *prompt, char *value, size_t size, int allow_digits)
{
    while (1)
    {
        get_string(prompt, value, size);

        if (valid_text(value, allow_digits))
            return;

        if (allow_digits)
            printf("Use at least two letters, and only letters, digits, spaces, - ' and . characters.\n");
        else
            printf("Use at least two letters, and only letters, spaces, - ' and . characters.\n");
    }
}

int id_search(int id)
{
    for (int i = 0; i < count; i++)
    {
        if (id == s[i].id)
            return i;
    }

    return -1;
}

/* Case-insensitive, the same way names are compared everywhere else */
int name_search(const char *name)
{
    for (int i = 0; i < count; i++)
    {
        if (strcasecmp(name, s[i].name) == 0)
            return i;
    }

    return -1;
}

/* One shared classifier so the risk tiers are defined in a single place */
int risk_level(float grade)
{
    if (grade < CRITICAL_LIMIT)
        return CRITICAL;
    if (grade < HIGH_LIMIT)
        return HIGH;
    if (grade < MODERATE_LIMIT)
        return MODERATE;
    if (grade < PASS_MARK)
        return LOW;

    return PASSED;
}

/* Finds the class average and the highest and lowest students. Call it only when count > 0 */
void class_stats(float *average, students **highest, students **lowest)
{
    students *ptr;
    float total = 0;

    *highest = s;
    *lowest = s;

    for (ptr = s; ptr < s + count; ptr++)
    {
        total += ptr->grade;

        if (ptr->grade > (*highest)->grade)
            *highest = ptr;

        if (ptr->grade < (*lowest)->grade)
            *lowest = ptr;
    }

    *average = total / count;
}

/* Highest grade first. Equal grades fall back to the lower ID so the order is always the same */
int compare_grade(const void *a, const void *b)
{
    const students *stud_a = a;
    const students *stud_b = b;

    if (stud_a->grade > stud_b->grade)
        return -1;

    if (stud_a->grade < stud_b->grade)
        return 1;

    if (stud_a->id < stud_b->id)
        return -1;

    if (stud_a->id > stud_b->id)
        return 1;

    return 0;
}

int compare_id(const void *a, const void *b)
{
    const students *student_a = a;
    const students *student_b = b;

    if (student_a->id < student_b->id)
        return -1;

    if (student_a->id > student_b->id)
        return 1;

    return 0;
}

/* Case-insensitive like the name search, so "amina" sorts before "Zoe" */
int compare_name(const void *a, const void *b)
{
    const students *student_a = a;
    const students *student_b = b;
    int result = strcasecmp(student_a->name, student_b->name);

    if (result != 0)
        return result;

    if (student_a->id < student_b->id)
        return -1;

    if (student_a->id > student_b->id)
        return 1;

    return 0;
}

void search_student()
{
    int id, search_type;
    char stud_name[50];
    int index = -1;
    if (get_number("How would you like to search for the students?\n0.Using a Student ID\n1.Using a Student name\nType in a number between 0-1: ", &search_type, 'i')!=1)
        return;
    if (search_type >1 || search_type < 0)
    {
        printf("Input a value between 0 - 1\n");
        return;
    }
    if (search_type == 0)
    {
        if (get_number("Student ID: ", &id, 'i') != 1)
                return;
        index = id_search(id);
        if (index == -1)
        {
            printf("This student ID doesn't exist in our system.\n");
            return;
        }
    }
    else
    {
        get_string("Student's Full Name: ", stud_name, sizeof(stud_name));
        index = name_search(stud_name);
        if (index == -1)
        {
            printf("This student name doesn't exist in our system.\n");
            return;
        }
    }
    printf("\nStudent found!\nStudent ID: %d\nStudent name: %s\nStudent Course: %s\nStudent Grade: %.2f\n", s[index].id, s[index].name, s[index].course, s[index].grade);
}

void add_student()
{
    students new_student;
    int id;

    if (get_number("Student ID: ", &id, 'i') != 1)
        return;

    if (id < 0)
    {
        printf("Student ID cannot be negative.\n");
        return;
    }

    if (id_search(id) != -1)
    {
        printf("Student already exists in our system\n");
        return;
    }
    new_student.id = id;

    /* Names are unique, so a name search can never be ambiguous */
    while (1)
    {
        get_name("Student Name: ", new_student.name, sizeof(new_student.name), 0);

        if (name_search(new_student.name) == -1)
            break;

        printf("A student with this name already exists in our system.\n");
    }

    get_name("Student Course: ", new_student.course, sizeof(new_student.course), 1);

    /* Ask for the grade again instead of throwing away the whole record */
    while (1)
    {
        if (get_number("Student Grade: ", &new_student.grade, 'f') != 1)
            return;

        if (new_student.grade >= 0 && new_student.grade <= 100)
            break;

        printf("Input a grade between 0 and 100\n");
    }

    /* Only grow the array now that the record is valid and will be kept */
    if (count >= capacity)
    {
        students *temp;
        temp = realloc(s, (capacity * 2)  * sizeof(students));
        if (temp == NULL)
        {
            printf("Memory allocation failed.\n");
            return;
        }
        s = temp;
        capacity *= 2;
    }

    s[count] = new_student;
    printf("\nNew student added!\nStudent ID: %d\nStudent name: %s\nStudent Course: %s\nStudent Grade: %.2f\n", s[count].id, s[count].name, s[count].course, s[count].grade);

    count++;

}

void display_students(void)
{
    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    printf("\n--------------------------------------------------------------------------\n");
    printf("%-10s %-25s %-25s %-10s\n",
           "ID", "NAME", "COURSE", "GRADE");
    printf("--------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++)
    {
        printf("%-10d %-25s %-25s %-10.2f\n",
               s[i].id,
               s[i].name,
               s[i].course,
               s[i].grade);
    }

    printf("--------------------------------------------------------------------------\n");
    printf("Total students: %d\n", count);
}
void remove_student()
{
    int id,pos,index;
    char choice[7];

    if (get_number("ID of the student you would love to remove: ", &id, 'i') != 1)
        return;

    /* Check the ID first, so the user is only asked to confirm a student that exists */
    index = id_search(id);
    if (index == -1)
    {
        printf("Student ID doesnt exists in our system\n");
        return;
    }

    get_string("Are you sure you want to proceed?\nType in 'yes' to proceed and any other letter to go back to the main menu: ", choice, sizeof(choice));

    if (strcasecmp(choice, "yes") != 0)
        return;

    /* Shift every record after the removed one back by one place */
    for (pos = index; pos < count - 1; pos ++)
        s[pos] = s[pos+1];

    count --;
    printf("\nStudent removed succesfully!\n");

    /* Shrink to exactly half. count <= capacity/2 means the records still fit.
       The capacity > 1 check stops it reaching 0, because realloc(s, 0) would free the array */
    if (capacity > 1 && count <= capacity/2)
    {
        students *temp;
        temp = realloc(s, (capacity / 2) * sizeof(students));
        if (temp == NULL)
        {
            printf("Memory allocation failed.\n");
            return;
        }
        s = temp;
        capacity /= 2;
    }
}

/* Explains what the custom analysis measures, how it is worked out and how to read it */
void risk_explanation(void)
{
    printf("\nHOW THE ACADEMIC RISK ANALYSIS WORKS\n");
    printf("Risk index: shows how far each student is from the pass mark of %d.\n", PASS_MARK);
    printf("Formula: (%d - grade) / %d * 100, or 0 when the grade is %d or higher.\n", PASS_MARK, PASS_MARK, PASS_MARK);
    printf("Reading it: 0 means no risk, 100 means a grade of 0. A higher index means more marks are needed to pass.\n");
    printf("Tiers: CRITICAL below %d, HIGH below %d, MODERATE below %d, LOW below %d, otherwise NO RISK.\n",
           CRITICAL_LIMIT, HIGH_LIMIT, MODERATE_LIMIT, PASS_MARK);
    printf("Course comparison: shows each course's student count, average, lowest and highest grade,\n");
    printf("and how many students are below the pass mark. A course with a low average or a wide gap\n");
    printf("between lowest and highest grade may need extra support.\n");
}

/* Compares courses: how many students, their average grade, lowest and highest grade, and how many are below the pass mark */
void course_breakdown(void)
{
    int i, j, seen, n, at_risk;
    float sum, low, high;

    printf("\nCOURSE COMPARISON\n");
    printf("------------------------------------------------------------------------------\n");
    printf("%-25s %-8s %-10s %-10s %-10s %-8s\n", "COURSE", "COUNT", "AVERAGE", "LOWEST", "HIGHEST", "AT RISK");

    for (i = 0; i < count; i++)
    {
        /* Skip a course we have already reported */
        seen = 0;
        for (j = 0; j < i; j++)
        {
            if (strcasecmp(s[i].course, s[j].course) == 0)
            {
                seen = 1;
                break;
            }
        }
        if (seen)
            continue;

        n = 0;
        sum = 0;
        at_risk = 0;
        low = 100;
        high = 0;

        for (j = 0; j < count; j++)
        {
            if (strcasecmp(s[i].course, s[j].course) == 0)
            {
                n++;
                sum += s[j].grade;

                if (s[j].grade < low)
                    low = s[j].grade;

                if (s[j].grade > high)
                    high = s[j].grade;

                if (s[j].grade < PASS_MARK)
                    at_risk++;
            }
        }

        printf("%-25s %-8d %-10.2f %-10.2f %-10.2f %-8d\n", s[i].course, n, sum / n, low, high, at_risk);
    }
}

void risk_index_calc()
{
    students *ptr;
    students *highest;
    students *lowest;
    float risk_index;
    float improvement;
    float average;
    int level;
    int tiers[TIERS] = {0};

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    class_stats(&average, &highest, &lowest);

    printf("\n========== ACADEMIC RISK ASSESSMENT ==========\n");
    risk_explanation();

    for (ptr = s; ptr < s + count; ptr++)
    {
        improvement = PASS_MARK - ptr->grade;

        level = risk_level(ptr->grade);
        tiers[level]++;

        printf("\nStudent: %s\n", ptr->name);
        printf("Risk Level: %s\n", tier_names[level]);

        /* Calculate numerical risk index */
        if (ptr->grade < PASS_MARK)
            risk_index = (improvement / PASS_MARK) * 100;
        else
            risk_index = 0;

        printf("Student ID: %d\n", ptr->id);
        printf("Current Grade: %.2f\n", ptr->grade);
        printf("Risk Index: %.2f/100\n", risk_index);

        if (improvement > 0)
            printf("Marks needed to pass: %.2f\n", improvement);
        else
            printf("Student has achieved the pass mark.\n");

        printf("----------------------------------------------\n");
    }

    printf("\n========== CLASS RISK SUMMARY ==========\n");
    printf("Total students: %d\n", count);
    printf("Class average: %.2f\n", average);

    printf("\nRisk distribution:\n");
    printf("Critical risk: %d\n", tiers[CRITICAL]);
    printf("High risk: %d\n", tiers[HIGH]);
    printf("Moderate risk: %d\n", tiers[MODERATE]);
    printf("Low risk: %d\n", tiers[LOW]);
    printf("Passed: %d\n", tiers[PASSED]);

    printf("\nStudent requiring the most support:\n");
    printf("Name: %s\n", lowest->name);
    printf("Grade: %.2f\n", lowest->grade);
    printf("Marks needed to pass: %.2f\n", PASS_MARK - lowest->grade);

    course_breakdown();

    printf("========================================\n");
}

void sort_students()
{
    int sort_type;
    students *sorted;

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    if (get_number("What do you want to sort the student records by?\n0.Student ID: \n1.Students Grades: \n2.Student Names: \nType in a number between 0-2: ", &sort_type, 'i')!=1)
        return;
    if (sort_type >2 || sort_type < 0)
    {
        printf("Input a value between 0 - 2\n");
        return;
    }

    /* Sort a copy so the stored records keep their original order */
    sorted = malloc(count * sizeof(students));
    if (sorted == NULL)
    {
        printf("Memory allocation failed.\n");
        return;
    }
    memcpy(sorted, s, count * sizeof(students));

    int (*sort_func[3]) (const void *a,const void *b) = {compare_id, compare_grade, compare_name};
    qsort(sorted, count, sizeof(students),sort_func[sort_type]);
    printf("\nSorted succesfully, here's the new order of records\n");
    for (int i = 0; i < count; i++)
    {
        printf("%d, %s, %s, %.2f\n", sorted[i].id, sorted[i].name, sorted[i].course, sorted[i].grade);
    }

    free(sorted);
}

void avg_grade()
{
    students *ptr;
    float sum = 0;

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    for (ptr = s; ptr < s + count; ptr++)
        sum += ptr->grade;

    printf("\nThe Average Grade of the students is: %.2f\n", sum / count);
}

void top_student()
{
    students *ptr;
    students *top;

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    top = s;
    for (ptr = s; ptr < s + count; ptr++)
    {
        if (ptr->grade > top->grade)
            top = ptr;
    }

    printf("\nThe top student is:\n");
    printf("%s, taking a course in %s, with a grade of %.2f\n", top->name, top->course, top->grade);


}

void summary_report(void)
{
    students *ptr;
    students *highest;
    students *lowest;

    float average;

    int passed = 0;
    int failed = 0;
    int tiers[TIERS] = {0};

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    class_stats(&average, &highest, &lowest);

    /* Traverse the student records using pointer arithmetic */
    for (ptr = s; ptr < s + count; ptr++)
    {
        /* Pass and fail count */
        if (ptr->grade >= PASS_MARK)
            passed++;
        else
            failed++;

        /* Custom risk analysis */
        tiers[risk_level(ptr->grade)]++;
    }

    /* Display summary report */
    printf("\n");
    printf("============================================\n");
    printf("          ACADEMIC SUMMARY REPORT           \n");
    printf("============================================\n");

    printf("\nGENERAL STATISTICS\n");
    printf("--------------------------------------------\n");
    printf("Total student records: %d\n", count);
    printf("Average grade: %.2f\n", average);

    printf("\nHIGHEST PERFORMING STUDENT\n");
    printf("--------------------------------------------\n");
    printf("Name: %s\n", highest->name);
    printf("Student ID: %d\n", highest->id);
    printf("Grade: %.2f\n", highest->grade);

    printf("\nLOWEST PERFORMING STUDENT\n");
    printf("--------------------------------------------\n");
    printf("Name: %s\n", lowest->name);
    printf("Student ID: %d\n", lowest->id);
    printf("Grade: %.2f\n", lowest->grade);

    printf("\nPASS AND FAIL STATISTICS\n");
    printf("--------------------------------------------\n");
    printf("Students who passed: %d\n", passed);
    printf("Students below pass threshold: %d\n", failed);

    printf("\nCUSTOM ANALYSIS: ACADEMIC RISK DISTRIBUTION\n");
    printf("--------------------------------------------\n");
    printf("Critical risk: %d\n", tiers[CRITICAL]);
    printf("High risk: %d\n", tiers[HIGH]);
    printf("Moderate risk: %d\n", tiers[MODERATE]);
    printf("Low risk: %d\n", tiers[LOW]);
    printf("(Use option 7 for the full risk index and the course comparison.)\n");

    printf("\n============================================\n");
}

int main(void)
{
//create our own dataset
    capacity = 13;
    count = 13;
    s = malloc(capacity * sizeof(students));
    if (s == NULL)
    {
        printf("Memory allocation failed\n");
        return (1);
    }

    srand((unsigned int)time(NULL));//seeding rand to fill the grades
    char *names[13] = {
    "Amina Wanjiku", "Brian Kamau", "Carol Atieno","David Mwangi", "Esther Nyambura", "Felix Otieno",
    "Grace Njeri", "Hannah Wairimu", "Isaac Kiptoo","Joy Achieng", "Kevin Mutua", "Linda Chebet","Michael Ochieng"
    };
    char *courses[4] = {"Software Engineering","Computer Science","Cybersecurity","Artificial Intelligence"};
    for (int i = 0; i < count; i++)
    {
        s[i].id = i;
        s[i].grade = (100 + rand() % 9901) / 100.0f;
        snprintf(s[i].name, sizeof(s[i].name), "%s", names[i]);
        snprintf(s[i].course, sizeof(s[i].course), "%s", courses[rand()%4]);
    }

    //initialize an array of funtion pointers
    void (*fun_ptr[])(void) = {add_student, search_student, sort_students, display_students, avg_grade, top_student, remove_student, risk_index_calc, summary_report};
    int choice;
    do
    {
            /* PROGRAM OUTPUT HEADER */
        printf("\n============================================\n");
        printf("Student ID: AK1723254\n");
        printf("Project: Dynamic Academic Performance and Risk Analysis Engine\n");
        printf("Custom Analysis: Academic Risk Index and Course Comparison\n");
        printf("============================================\n");
        printf("Menu:\n");
        printf("0. Add Student\n");
        printf("1. Search for a Student\n");
        printf("2. Sort Students\n");
        printf("3. Display Student Records\n");
        printf("4. Calculate Average Grade\n");
        printf("5. Identify the top-performing student\n");
        printf("6. Remove student\n");
        printf("7. Academic risk analysis and course comparison\n");
        printf("8. Generate summary report\n");
        printf("9. Exit\n");
        if (get_number("Enter your choice: ", &choice, 'i')!=1)
        {
            free(s);
            return 1;
        }

        if (choice == 9)
        {
            printf("Exiting...\n");
            break;
        }
        if (choice >= 0 && choice < 9)
            fun_ptr[choice]();
        else
            printf("Invalid choice.\n");

    } while (choice != 9);
    free(s);
    return 0;
}