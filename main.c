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

typedef struct StudentInfo{
  int id;
  int visits;
  int help;
  pthread_t studentThread;
} Student;

int students = 0;
int tutors = 0;
int chairs = 0;
int help = 0;

// 2 rows X MAX students coloumns
// First row to hold studentID
// Second row to hold number of visits
int priorityQueue[2][MAX];
int helpNumber[MAX]; //each index represents student ID
int chairsAvailable = 0;
int visits[MAX];
int done = 0; // Needs to hit the number of students
int *currentStudent;
int totalRequests = 0;
int studentIDs[MAX];

unsigned int seed = 86;
 
sem_t semStudent;
sem_t semTutor;
pthread_mutex_t chairLock; // Only student can access chairs

int main(int argc, const char * argv[]) {
    students = atoi(argv[1]);
    tutors = atoi(argv[2]);
    chairs = atoi(argv[3]);
    help = atoi(argv[4]);
    
    sem_init(&semStudent, 0, 0);
    sem_init(&semTutor, 0, 0);
    pthread_mutex_init(&chairLock, NULL);
    
    pthread_t student_threads[students];
    pthread_t tutor_threads[tutors];
    pthread_t coordinator_threads;

    // create coordinator thread
    pthread_create(&coordinator_threads, NULL, coordinatorFunc, NULL);
    // Initializing all the variables needed to keep track
    for(int i = 0; i < students; i++){
      priorityQueue[0][i] = -1; // -1 for empty
      priorityQueue[1][i] = -1; 
      helpNumber[i] = help; // Starting number of helps needed
      visits[i] = 0;
      currentStudent = malloc(sizeof(*currentStudent));
    }
    chairsAvailable = chairs; // Starting number of chairs
    // create student threads
    for(int i = 0; i < students; i++) {
        studentIDs[i] = i;
        pthread_create(&student_threads[i], NULL, studentFunc, (void *) &studentIDs[i]);
    }
    /*
    // create tutor threads
    for(int i = 0;  i < tutors; i++) {
        pthread_create(&tutor_threads[i], NULL, tutorFunc, (void *) &i);
    }
    */
    
    // join coordinator thread
    pthread_join(coordinator_threads, NULL);
    // join student threads
    for(int i = 0; i < students; i++) {
        pthread_join(student_threads[i], NULL);
    }
    /*
    // join tutor threads
    for(int i = 0; i < tutors; i++) {
        pthread_join(tutor_threads[i], NULL);
    }
    */
    

    printf("Done\n");
    
    return 0;
}

/*  1) Student checks if there are chairs available
*     1.a) Available = S.visits++ and Coord. places priorityQ
*     1.b) Full = student leaves and S.visit++
*   2) Students wait until tutor checks priorityQ
*     2.a) Tutor takes in student: chairsAvailable++
*     2.b) Tutor returns student: S.helpNumber[i] == subtract 1
*/
void *studentFunc(void *id) {
    int studentID = *(int*) id;
    while(1) {
        // Put the student to sleep 
        int waitingTime = (rand_r(&seed) % 2) + 1;
        sleep(waitingTime);

        // Check if any students are done, then kill students
        if(helpNumber[studentID] <= 0){
          pthread_mutex_lock(&chairLock);
          done++;
          pthread_mutex_unlock(&chairLock);
          // Notify the coordinator the student is done
          sem_post(&semStudent);
          // Kill the thread
          printf("Student %d is done\n", studentID);
          return NULL;
        }
        
        // lock before accessing the number of chairs
        pthread_mutex_lock(&chairLock);
        // no chair available
        if(chairsAvailable <= 0) {
            printf("S: Student %d found no empty chair. Will try again later.\n", studentID);
            pthread_mutex_unlock(&chairLock);
            continue;
        }
        
        // found an empty chair
        chairsAvailable--;
        // increase visit of student by 1
        visits[studentID]++;
        totalRequests++;
        *currentStudent = studentID;

        printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentID, chairsAvailable);
        helpNumber[studentID] = helpNumber[studentID] - 1;
        printf("Student %d got help. Help needed is %d\n",*currentStudent,helpNumber[studentID]);

        pthread_mutex_unlock(&chairLock);
        // notify the coordinator the student is seated
        sem_post(&semStudent);

    }
}

/*  1) Gets notified by Coord.
*     1.a) Check priorityQ at index [][0]
*     1.b) Remove the student at said index, shift everything down
*/
void *tutorFunc(void *id) {
    int tutorID = *(int*) id;
    // printf("Created tutor thread: %d\n", tutorID);
    while(1){
      // Check if all students are done, kill tutors
      if(students == done){
        return NULL;
      }

      // Wait for Coord. to notify tutor
      sem_wait(&semTutor);

    }
}

/*  1) When a student is seated
*     1.a) Update priorityQ
*          Insert by # of visits
*          Equal visits get inserted before the next highest visits or the end
*          Shift everything down
*   2) Notify the tutors that are not busy
*/
void *coordinatorFunc(void) {
    // printf("Created coordinator thread\n");
    while(1) {
        // Kill Coord.
        if(done >= students){
          // Kill the thread
          printf("All students queued\n");
          return NULL;
        }

        // waiting for a student to get queued
        sem_wait(&semStudent);

        // Update the priorityQ
        pthread_mutex_lock(&chairLock);
        // [0][i] holds studentID
        // [1][i] holds visits[studentID]
        // Traverse the entire queue and compare number of visits
        // Place the current student id
        int tempID = -1;
        int tempVisits = -1;
        
        printf("Q: Student %d with %d visits is being queued\n", *currentStudent,visits[*currentStudent]);
        // Inserting
        for(int i = 0; i < students; i++){
          if(priorityQueue[0][i] == -1 && priorityQueue[1][i] == -1){
            priorityQueue[0][i] = *currentStudent;
            priorityQueue[1][i] = visits[*currentStudent];
            break;
          }
        }

        // Sorting by number of visits
        for(int i = 0; i < students; i++){
          if(priorityQueue[0][i] == -1 && priorityQueue[1][i] == -1){
            break;
          }
          for(int j = i + 1; j < students; j++){
            if(priorityQueue[0][j] == -1 && priorityQueue[1][j] == -1){
              break;
            }
            if(priorityQueue[1][j] < priorityQueue[1][i]){
              tempID = priorityQueue[0][i];
              tempVisits = priorityQueue[1][i];
              priorityQueue[0][i] = priorityQueue[0][j];
              priorityQueue[1][i] = priorityQueue[1][j];
              priorityQueue[0][j] = tempID;
              priorityQueue[1][j] = tempVisits;
            }
          }
        }
        chairsAvailable++;
        printf("Freed a chair. Chairs now %d\n",chairsAvailable);
        printf("Current order: \n");
        for(int x=0; x<students; x++)
        {
            printf("(%d,%d) ", priorityQueue[0][x],priorityQueue[1][x]);
        }
	      printf("\n");
        // notify the tutor the queue is updated
        //sem_post(&semTutor);
        pthread_mutex_unlock(&chairLock);
  
    }
}