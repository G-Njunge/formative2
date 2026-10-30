#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>

typedef struct{
    int id;
    char name[50];
    char course[50];
    float grade;

} students;

students *s;
int capacity, count;
//struct for each student
int get_number(const char *prompt, void *value, char type)
{
    int result;
    int ch;

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

        while ((ch = getchar()) != '\n' && ch != EOF)
        {
            if (ch != ' ' && ch != '\t')
            {
                printf("Invalid input. Try again.\n");

                while ((ch = getchar()) != '\n' && ch != EOF)
                    ;

                break;
            }
        }

        if (ch == '\n' || ch == EOF)
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
            exit(0);

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

int id_search(int id)
{
    for (int i = 0; i < count; i++)
    {
        if (id == s[i].id)
            return i;
    }

    return -1;
}

int compare_grade(const void *a, const void *b)
{
    const students *stud_a = a;
    const students *stud_b = b;

    if (stud_a->grade > stud_b->grade)
        return -1;

    if (stud_a->grade < stud_b->grade)
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

int compare_name(const void *a, const void *b)
{
    const students *student_a = a;
    const students *student_b = b;

    return strcmp(student_a->name, student_b->name);
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
        printf("Input a value between 0 - 1");
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
        for (int i = 0; i < count; i++)
        {
            if (strcasecmp(stud_name, s[i].name) == 0)
                index = i;
        }
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
    int id,ch;

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

    if (get_number("Student ID: ", &id, 'i') != 1)
        return;

    int index = id_search(id);
    if (index == -1)
        s[count].id = id;
    else
    {
        printf("Student already exists in our system\n");
        return;
    }

    get_string("Student Name: ", s[count].name, sizeof(s[count].name));

    get_string("Student Course: ", s[count].course, sizeof(s[count].course));

    if (get_number("Student Grade: ", &s[count].grade, 'f')!=1)
        return;
    if (s[count].grade >= 0 && s[count].grade <= 100)
        printf("\nNew student added!\nStudent ID: %d\nStudent name: %s\nStudent Course: %s\nStudent Grade: %.2f\n", s[count].id, s[count].name, s[count].course, s[count].grade);
    else
    {
        printf("Input a grade between 0 and 100\n");
        return;
    }

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
    int id,pos;
    char choice[4];

    if (count <= capacity/2)
    {
        students *temp;
        temp = realloc(s, (capacity / 2)  * sizeof(students));
        if (temp == NULL)
        {
            printf("Memory allocation failed.\n");
            return;
        }
        s = temp;
        capacity /= 2;
    }

    if (get_number("ID of the student you would love to remove: ", &id, 'i') != 1)
        return;
    get_string("Are you sure you want to proceed?\nType in 'yes'to proceed and any other letter to go back to the main menu", choice, sizeof(choice));

    if (strcasecmp(choice, "yes") != 0)
        return;
    int index = id_search(id);
    if (index == -1)
    {
        printf("Student ID doesnt exists in our system\n");
        return;
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            if(id == s[i].id)
            {
                for (pos = i; pos < count -1; pos ++)
                {
                    s[pos].id = s[pos+1].id;
                    strcpy(s[pos].name,s[pos+1].name);
                    strcpy(s[pos].course,s[pos+1].course);
                    s[pos].grade = s[pos+1].grade;
                }
            }

        }
    }
    count --;
    printf("Student removed succesfully!");
}

void risk_index_calc()
{
    students *ptr;
    students *lowest;
    float risk_index;
    float improvement;
    float total_grade = 0;
    float average;

    int critical = 0;
    int high = 0;
    int moderate = 0;
    int low = 0;
    int passed = 0;

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    lowest = s;

    printf("\n========== ACADEMIC RISK ASSESSMENT ==========\n");

    for (ptr = s; ptr < s + count; ptr++)
    {
        total_grade += ptr->grade;

        if (ptr->grade < lowest->grade)
            lowest = ptr;

        improvement = 60 - ptr->grade;

        if (ptr->grade < 20)
        {
            critical++;
            printf("\nStudent: %s\n", ptr->name);
            printf("Risk Level: CRITICAL\n");
        }
        else if (ptr->grade < 40)
        {
            high++;
            printf("\nStudent: %s\n", ptr->name);
            printf("Risk Level: HIGH\n");
        }
        else if (ptr->grade < 50)
        {
            moderate++;
            printf("\nStudent: %s\n", ptr->name);
            printf("Risk Level: MODERATE\n");
        }
        else if (ptr->grade < 60)
        {
            low++;
            printf("\nStudent: %s\n", ptr->name);
            printf("Risk Level: LOW\n");
        }
        else
        {
            passed++;
            printf("\nStudent: %s\n", ptr->name);
            printf("Risk Level: NO RISK - PASS\n");
        }

        /* Calculate numerical risk index */
        if (ptr->grade < 60)
            risk_index = (improvement / 60) * 100;
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

    average = total_grade / count;

    printf("\n========== CLASS RISK SUMMARY ==========\n");
    printf("Total students: %d\n", count);
    printf("Class average: %.2f\n", average);

    printf("\nRisk distribution:\n");
    printf("Critical risk: %d\n", critical);
    printf("High risk: %d\n", high);
    printf("Moderate risk: %d\n", moderate);
    printf("Low risk: %d\n", low);
    printf("Passed: %d\n", passed);

    printf("\nStudent requiring the most support:\n");
    printf("Name: %s\n", lowest->name);
    printf("Grade: %.2f\n", lowest->grade);
    printf("Marks needed to pass: %.2f\n", 60 - lowest->grade);

    printf("========================================\n");
}

void sort_students()
{
    int sort_type;
    if (get_number("What do you want to sort the student records by?\n0.Student ID: \n1.Studenst Grades: \n2.Student Names: \nType in a number between 0-2: ", &sort_type, 'i')!=1)
        return;
    if (sort_type >2 || sort_type < 0)
    {
        printf("Input a value between 0 - 2");
        return;
    }
    int (*sort_func[3]) (const void *a,const void *b) = {compare_id, compare_grade, compare_name};
    qsort(s, count, sizeof(students),sort_func[sort_type]);
    printf("\nSorted succesfully, here's the new order of records\n");
    for (int i = 0; i < count; i++)
    {
        printf("%d, %s, %s, %.2f\n", s[i].id, s[i].name, s[i].course, s[i].grade);
    }
}

void avg_grade()
{
    float sum = 0;
    for (int i = 0; i < count; i++)
        sum += s[i].grade;
    printf("\nThe Average Grade of the students is: %.2f\n", sum / count);
}

void top_student()
{
    int max = 0;
    int pos = 0;
    for (int i = 0; i < count; i++)
    {
        if (max < s[i].grade)
        {
            max = s[i].grade;
            pos = i;
        }
    }

    printf("\nThe top student is:\n");
    printf("%s, taking a course in %s, with a grade of %.2f\n", s[pos].name, s[pos].course, s[pos].grade);


}

void summary_report(void)
{
    students *ptr;
    students *highest;
    students *lowest;

    float total = 0;
    float average;

    int passed = 0;
    int failed = 0;

    int critical = 0;
    int high = 0;
    int moderate = 0;
    int low = 0;

    if (count == 0)
    {
        printf("No student records available.\n");
        return;
    }

    highest = s;
    lowest = s;

    /* Traverse the student records using pointer arithmetic */
    for (ptr = s; ptr < s + count; ptr++)
    {
        total += ptr->grade;

        if (ptr->grade > highest->grade)
            highest = ptr;

        if (ptr->grade < lowest->grade)
            lowest = ptr;

        /* Pass and fail count */
        if (ptr->grade >= 60)
            passed++;
        else
            failed++;

        /* Custom risk analysis */
        if (ptr->grade < 20)
            critical++;
        else if (ptr->grade < 40)
            high++;
        else if (ptr->grade < 50)
            moderate++;
        else if (ptr->grade < 60)
            low++;
    }

    average = total / count;

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
    printf("Critical risk: %d\n", critical);
    printf("High risk: %d\n", high);
    printf("Moderate risk: %d\n", moderate);
    printf("Low risk: %d\n", low);

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
    int choice, ch;
    do
    {
        printf("\nMenu:\n");
        printf("0. Add Student\n");
        printf("1. Search for a Student\n");
        printf("2. Sort Students\n");
        printf("3. Display Student Records\n");
        printf("4. Calculate Average Grade\n");
        printf("5. Identify the top-performing student\n");
        printf("6. Remove student\n");
        printf("7. Calculate risk score based on the student's grade\n");
        printf("8. Generate summary report\n");
        printf("9. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        while ((ch = getchar()) != '\n' && ch != EOF);

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
}
