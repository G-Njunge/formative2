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

void search_student()
{
    int id, result;
    do
    {
        printf("Student ID: ");
        result = scanf("%d", &id);

        if (result == EOF)
        {
            printf("\nInput ended. System terminated.\n");
            break;
        }
        if (result != 1)
        {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
            printf("\ninvalid option, please enter a number from 1 to 5\n");
            continue;
        }
    }
    while (result != 1);
    int found = 1;
    for (int i = 0; i < count; i++)
    {
        if (id == s[i].id)
            found = 0;
    }

    if (found != 0)
    {
        printf("This student ID doesnt exist in our system");
        return;
    }

    printf("\nStudent details\nStudent ID: %d\nStudent name: %s\nStudent Course: %s\nStudent Grade: %f\n", s[id].id, s[id].name, s[id].course, s[id].grade);





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

    // printf("%d, %f, %s, %s", s[0].id,s[0].grade,s[0].course,s[0].name);

    //initialize an array of funtion pointers
    void (*fun_ptr)(void) = search_student;
    fun_ptr();
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
