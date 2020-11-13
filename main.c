#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <unistd.h>
#define MAX 2100

void *studentFunc(void *id);
void *tutorFunc(void *id);
void *coordinatorFunc();

int students = 0;
int tutors = 0;
int chairs = 0;
int help = 0;

// 2 rows X MAX students coloumns
// First row to hold studentID
// Second row to hold number of visits
int *priorityQueue;
int *helpNumber;	// number of help the student at that index need
int *visits;	// number of times the student at that index visited
int *studentIDs;	// student ID
int *tutorIDs;	// tutor ID
int *tutor;	// tutor that tutored the student at that index
int chairsAvailable = 0;
int done = 0;	// Needs to hit the number of students
int *currentStudent;
int totalRequests = 0;
int tutoredNow = 0;
int totalSession = 0;

unsigned int seed = 86;
int studentCount = 0;
int tutorCount = 0;
int coordCount = 0;

sem_t semStudent;
sem_t semTutor;
pthread_mutex_t chairLock;	// Only student can access chairs
//pthread_mutex_t queueLock;	// Only coord. and tutor can access the priority queue

int main(int argc, const char *argv[])
{
    students = atoi(argv[1]);
    tutors = atoi(argv[2]);
    chairs = atoi(argv[3]);
    help = atoi(argv[4]);

    sem_init(&semStudent, 0, 0);
    sem_init(&semTutor, 0, 0);
    pthread_mutex_init(&chairLock, NULL);
    //pthread_mutex_init(&queueLock, NULL);

    pthread_t student_threads[students];
    pthread_t tutor_threads[tutors];
    pthread_t coordinator_threads;

    // create coordinator thread
    pthread_create(&coordinator_threads, NULL, coordinatorFunc, NULL);

    currentStudent = (int *)malloc(students *sizeof(int));
    studentIDs = (int *)malloc(students *sizeof(int));
    tutorIDs = (int *)malloc(tutors *sizeof(int));
    helpNumber = (int *)malloc(students *sizeof(int));
    visits = (int *)malloc(students *sizeof(int));
    tutor = (int *)malloc(students *sizeof(int));
    int queueSize = students * 2;
    priorityQueue = (int *)malloc(queueSize *sizeof(int));
    // Initializing all the variables needed to keep track
    for (int i = 0; i < students; i++)
    {
        priorityQueue[i] = -1;    // -1 for empty
        priorityQueue[i + students] = -1;
        helpNumber[i] = help;    // Starting number of helps needed
        visits[i] = 0;
        tutor[i] = -1;
        currentStudent[i] = -1;
    }
    chairsAvailable = chairs;    // Starting number of chairs

    // create student threads
    for (int i = 0; i < students; i++)
    {
        studentIDs[i] = i;
        pthread_create(&student_threads[i], NULL, studentFunc, (void*) &studentIDs[i]);
    }

    // create tutor threads
    for (int i = 0; i < tutors; i++)
    {
        tutorIDs[i] = i;
        pthread_create(&tutor_threads[i], NULL, tutorFunc, (void*) &tutorIDs[i]);
    }

    // join coordinator thread
    pthread_join(coordinator_threads, NULL);

    // join student threads
    for (int i = 0; i < students; i++)
    {
        pthread_join(student_threads[i], NULL);
    }

    // join tutor threads
    for (int i = 0; i < tutors; i++)
    {
        pthread_join(tutor_threads[i], NULL);
    }

    printf("Total request: %d\tTotal session: %d\n",totalRequests,totalSession);
    /*for(int i = 0; i < students; i++){
      printf("[ID:%d, Visits:%d, Help:%d]\n",i,visits[i],helpNumber[i]);
    }
    */

    return 0;
}

/* 1) Student checks if there are chairs available
 *    1.a) Available = S.visits++ and Coord. places priorityQ
 *    1.b) Full = student leaves and S.visit++
 *  2) Students wait until tutor checks priorityQ
 *    2.a) Tutor takes in student: chairsAvailable++
 *    2.b) Tutor returns student: S.helpNumber[i] == subtract 1
 */
void *studentFunc(void *id) {
    int studentID = *(int*) id;
    while (1) {
        // Put the student to sleep
        int waitingTime = (rand_r(&seed) % 2) + 1;
        sleep(waitingTime);

        // Check if any students are done, then kill students
        if (helpNumber[studentID] <= 0 ) {
            printf("****Student %d is done****\n", studentID);
            pthread_mutex_lock(&chairLock);
            done++;
            pthread_mutex_unlock(&chairLock);
            // Notify the coordinator the student is done
            sem_post(&semStudent);
            // Kill the thread
            return NULL;
        }

        // lock before accessing the number of chairs
        pthread_mutex_lock(&chairLock);
        // no chair available
        if (chairsAvailable <= 0) {
            //printf("S: Student %d found no empty chair. Will try again later.\n", studentID);
            pthread_mutex_unlock(&chairLock);
            continue;
        }

        // found an empty chair
        chairsAvailable--;
        // increase visit of student by 1
        visits[studentID]++;
        totalRequests++;
        for(int i = 0; i < students; i++) {
            if(currentStudent[i] == -1){
                currentStudent[i] = studentID;
                break;
            }
        }
        //printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentID, chairsAvailable);
        pthread_mutex_unlock(&chairLock);

        // notify the coordinator the student is seated
        sem_post(&semStudent);

        // wait for tutor
        while (tutor[studentID] == -1);

        //printf("S: Student %d received help from Tutor %d\n", studentID, tutor[studentID]);
        //printf("Student %d needs %d more help\n",studentID,helpNumber[studentID]);
        //pthread_mutex_lock(&chairLock);
        // helpNumber[studentID]--;
        // pthread_mutex_unlock(&chairLock);
    }
}

/* 1) Gets notified by Coord.
 *    1.a) Check priorityQ at index[][0]
 *    1.b) Remove the student at said index, shift everything down
 */
void *tutorFunc(void *id) {
    int tutorID = *(int*) id;
    while (1) {
        // Check if all students are done, kill tutors
        if (done >= students)
        {
            printf("****Tutor %d done****\n", tutorID);
            return NULL;
        }
        
        // Wait for Coord. to notify tutor
        sem_wait(&semTutor);

        pthread_mutex_lock(&chairLock);
        // student with highest priority
        int studentHP = priorityQueue[0];
        // no student in the list
        if(studentHP == -1) {
            pthread_mutex_unlock(&chairLock);
            continue;
        }
        // remove student with highest priority from priorityQ
        for (int i = 0; i < students; i++)
        {
            if (i == students - 1)
            {
                priorityQueue[i] = -1;
                priorityQueue[i+students] = -1;
            }
            else
            {
                priorityQueue[i] = priorityQueue[i + 1];
                priorityQueue[i+students] = priorityQueue[i +students+ 1];
            }
        }
    
        // update available chairs
        chairsAvailable++;
        //printf("Student %d is being tutored by tutor %d. Free chairs now %d\n", studentHP,tutorID, chairsAvailable);
        tutoredNow++;
        tutor[studentHP] = tutorID;
        pthread_mutex_unlock(&chairLock);

        /*
        * sleep time change to 0.2
        */
        int waitingTime = (rand_r(&seed) % 2) + 1;
        sleep(waitingTime);

        // after tutoring
        pthread_mutex_lock(&chairLock);
        tutoredNow--;
        totalSession++;
        //printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", studentHP, tutor[studentHP], tutoredNow, totalSession);
        tutor[studentHP] = -1;
        pthread_mutex_unlock(&chairLock);
    }
}

/* 1) When a student is seated
 *    1.a) Update priorityQ
 *         Insert by # of visits
 *         Equal visits get inserted before the next highest visits or the end
 *         Shift everything down
 *  2) Notify the tutors that are not busy
 */
void *coordinatorFunc(void) {
    int priority = 0;
    while (1) {
        // Kill Coord.
        if (done >= students){
            // notify all the tutor threads to terminate
            for (int i = 0; i < tutors; i++) {
                sem_post(&semTutor);
            }

            printf("****All students done****\n");
            return NULL;
        }
        
        // waiting for a student to get queued
        sem_wait(&semStudent);

        // Update the priorityQ
        pthread_mutex_lock(&chairLock);
        //[i] holds studentID
        //[i+students] holds visits[studentID]
        // Traverse the entire queue and compare number of visits
        // Place the current student id
        if(helpNumber[*currentStudent] > 0 ) {
          /*
            printf("Students waiting for queue: ");
            for(int i = 0; i < students; i++){
                if(currentStudent[i] != -1){
                    printf("(%d)",currentStudent[i]);
                }
            }
            printf("\n");
  
            printf("Q: Student %d with %d visits is being queued\n", 
            currentStudent[0], visits[currentStudent[0]]);
            */
            // Inserting
            for (int i = 0; i < students; i++){
                if (priorityQueue[i] == -1 && priorityQueue[i+students] == -1){
                    priority = i;
                    priorityQueue[i] = currentStudent[0];
                    priorityQueue[i+students] = visits[currentStudent[0]];
                    break;
                }
            }

            int tempID = -1;
            int tempVisits = -1;
            // Sorting by number of visits
            for (int i = 0; i < students; i++) {
                if (priorityQueue[i] == -1 && priorityQueue[i+students] == -1) {
                    break;
                }
                for (int j = i + 1; j < students; j++) {
                    if (priorityQueue[j] == -1 && priorityQueue[j+students] == -1) {
                        break;
                    }
                    if (priorityQueue[j+students] < priorityQueue[i+students]) {
                        priority = i;
                        tempID = priorityQueue[i];
                        tempVisits = priorityQueue[i+students];
                        priorityQueue[i] = priorityQueue[j];
                        priorityQueue[i+students] = priorityQueue[j+students];
                        priorityQueue[j] = tempID;
                        priorityQueue[j+students] = tempVisits;
                    }
                }
            }
        
            //printf("C: Student %d with priority %d in the queue. Waiting students now = %d. Total requests = %d\n", currentStudent[0], priority + 1, chairs - chairsAvailable, totalRequests);

            /*
            printf("Priority Queue now: ");
            for (int x = 0; x < students; x++) {
                if(priorityQueue[0][x] != -1 && priorityQueue[1][x] != -1)
                    printf("(%d,%d) ", priorityQueue[0][x], priorityQueue[1][x]);
            }
            printf("\n");
            */
            helpNumber[currentStudent[0]]--;
            // Remove the first student in current student list
            for(int i = 0; i < students; i++){
                if(i == students - 1){
                    currentStudent[i] = -1;
                }
                else{
                    currentStudent[i] = currentStudent[i+1];
                }
            }
            /*
            printf("students waiting for queue now: ");
            for(int i = 0; i < students; i++){
                if(currentStudent[i] != -1){
                    printf("(%d) ",currentStudent[i]);
                }
            }
            printf("\n");
            */
            // notify the tutor the queue is updated
            sem_post(&semTutor);
        }
        pthread_mutex_unlock(&chairLock);
    }
}