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
    void (*fun_ptr[])(void) = {add_student, search_student, sort_students, display_students, avg_grade, top_student, remove_student};
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
        printf("9. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        while ((ch = getchar()) != '\n' && ch != EOF);

        if (choice >= 0 && choice < 10)
            fun_ptr[choice]();
        else
            printf("Invalid choice.\n");

    } while (choice != 9);
    free(s);
//enable data set to grow or shrink i.e make it dynamic
//10 functionalities + exit
//array of function pointers for the ten
//allocate memory in a way that the meory can grow or shrink when students are added or subtracted
//ensure to use pointer arithmetic at list once
//string operation functions i.e strcmp
//one original analysis
//summary reported format as provided

}
