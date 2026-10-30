#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    size_t len;

    while (1)
    {
        printf("%s", prompt);

        if (fgets(value, size, stdin) == NULL)
            exit(0);

        len = strlen(value);

        /* Check whether the entire input fits */
        if (len > 0 && value[len - 1] != '\n')
        {
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;

            printf("Input too long. Try again.\n");
            continue;
        }

        /* Remove the newline */
        if (len > 0 && value[len - 1] == '\n')
            value[len - 1] = '\0';

        /* Reject empty input */
        if (value[0] == '\0')
        {
            printf("Input cannot be empty.\n");
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

void search_student()
{
    int id, index;
    if (get_number("Student ID: ", &id, 'i') != 1)
            return;
    index = id_search(id);
    if (index == -1)
    {
        printf("This student ID doesn't exist in our system.\n");
        return;
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
    void (*fun_ptr[])(void) = {add_student, search_student};
    int choice, ch;
    do
    {
        printf("\nMenu:\n");
        printf("0. Add Student\n");
        printf("1. Search for a Student\n");
        printf("2. Delete Record\n");
        printf("3. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        while ((ch = getchar()) != '\n' && ch != EOF);

        if (choice >= 0 && choice < 4)
            fun_ptr[choice]();
        else
            printf("Invalid choice.\n");

    } while (choice != 3);
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
