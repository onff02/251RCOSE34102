#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define MAX_PROCESSES 100
#define TIME_QUANTUM 4

// Process Structure
typedef struct {
    int pid;
    int arrival_time;
    int cpu_burst_time_initial;
    int io_request_time;
    int io_burst_time;
    int priority;

    int remaining_cpu_total;
    int cpu_done_current_segment;
    int remaining_cpu_after_io; 

    int start_time;             
    int completion_time;        
    int waiting_time;           
    int turnaround_time;        
    int response_time;          
    int last_active_time;       

    // State: 0: not_arrived, 1: ready, 2: running, 3: waiting_io, 4: completed
    int state;
    int io_complete_at_time;    
    int has_started_execution;  
    int current_quantum_slice;
} Process;

Process processes[MAX_PROCESSES];           
Process original_processes[MAX_PROCESSES]; 
int num_processes;

Process ready_queue[MAX_PROCESSES];
int ready_queue_size = 0;
Process waiting_queue[MAX_PROCESSES]; 
int waiting_queue_size = 0;

typedef struct {
    int pid;
    int start;
    int end;
} GanttEntry;
GanttEntry gantt_chart[MAX_PROCESSES * 20];
int gantt_idx = 0;


void sort_processes_by_arrival(Process p_arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (p_arr[j].arrival_time > p_arr[j + 1].arrival_time) {
                Process temp = p_arr[j];
                p_arr[j] = p_arr[j + 1];
                p_arr[j + 1] = temp;
            }
        }
    }
}

void add_to_ready_queue(Process p) {
    if (ready_queue_size < MAX_PROCESSES) {
        ready_queue[ready_queue_size++] = p;
    }
}

Process remove_from_ready_queue(int idx) {
    if (idx < 0 || idx >= ready_queue_size) {
        Process null_p = {-1}; return null_p;
    }
    Process p = ready_queue[idx];
    for (int i = idx; i < ready_queue_size - 1; i++) {
        ready_queue[i] = ready_queue[i+1];
    }
    ready_queue_size--;
    return p;
}

Process get_from_ready_queue_fcfs() {
    if (ready_queue_size == 0) { Process null_p = {-1}; return null_p;}

    int earliest_idx = 0;
    for(int i=1; i<ready_queue_size; ++i){

        if(ready_queue[i].arrival_time < ready_queue[earliest_idx].arrival_time){
             earliest_idx = i;
        }
    }
    return remove_from_ready_queue(earliest_idx);
}


Process get_from_ready_queue_sjf() {
    if (ready_queue_size == 0) { Process null_p = {-1}; return null_p; }
    int shortest_idx = 0;
    for (int i = 1; i < ready_queue_size; i++) {
        if (ready_queue[i].remaining_cpu_total < ready_queue[shortest_idx].remaining_cpu_total) {
            shortest_idx = i;
        } else if (ready_queue[i].remaining_cpu_total == ready_queue[shortest_idx].remaining_cpu_total) {
            if (ready_queue[i].arrival_time < ready_queue[shortest_idx].arrival_time) {
                 shortest_idx = i;
            }
        }
    }
    return remove_from_ready_queue(shortest_idx);
}

Process peek_ready_queue_sjf() {
    if (ready_queue_size == 0) { Process null_p = {-1}; return null_p; }
    int shortest_idx = 0;
    for (int i = 1; i < ready_queue_size; i++) {
        if (ready_queue[i].remaining_cpu_total < ready_queue[shortest_idx].remaining_cpu_total) {
            shortest_idx = i;
        } else if (ready_queue[i].remaining_cpu_total == ready_queue[shortest_idx].remaining_cpu_total) {
            if (ready_queue[i].arrival_time < ready_queue[shortest_idx].arrival_time) {
                 shortest_idx = i;
            }
        }
    }
    return ready_queue[shortest_idx];
}


Process get_from_ready_queue_priority() {
    if (ready_queue_size == 0) { Process null_p = {-1}; return null_p; }
    int highest_priority_idx = 0;
    for (int i = 1; i < ready_queue_size; i++) {
        if (ready_queue[i].priority < ready_queue[highest_priority_idx].priority) {
            highest_priority_idx = i;
        } else if (ready_queue[i].priority == ready_queue[highest_priority_idx].priority) {
            if (ready_queue[i].arrival_time < ready_queue[highest_priority_idx].arrival_time) {
                 highest_priority_idx = i;
            }
        }
    }
    return remove_from_ready_queue(highest_priority_idx);
}

Process peek_ready_queue_priority() {
    if (ready_queue_size == 0) { Process null_p = {-1}; return null_p; }
    int highest_priority_idx = 0;
    for (int i = 1; i < ready_queue_size; i++) {
        if (ready_queue[i].priority < ready_queue[highest_priority_idx].priority) {
            highest_priority_idx = i;
        } else if (ready_queue[i].priority == ready_queue[highest_priority_idx].priority) {
            if (ready_queue[i].arrival_time < ready_queue[highest_priority_idx].arrival_time) {
                 highest_priority_idx = i;
            }
        }
    }
    return ready_queue[highest_priority_idx];
}


Process get_from_ready_queue_rr() {
    if (ready_queue_size == 0) { Process null_p = {-1}; return null_p;}
    return remove_from_ready_queue(0);
}

void display_menu() {
    printf("         CPU SCHEDULING SIMULATOR\n\n");
    printf("1. Create random processes\n");
    printf("2. FCFS scheduling\n");
    printf("3. SJF (Non-Preemptive) scheduling\n");
    printf("4. SJF (Preemptive) scheduling\n");
    printf("5. Priority (Non-Preemptive) scheduling\n");
    printf("6. Priority (Preemptive) scheduling\n");
    printf("7. Round Robin scheduling\n");
    printf("0. Exit\n\n");
    printf("Choice: ");
}

void Create_Process() {
    printf("Enter number of processes (e.g., 5): ");
    if (scanf("%d", &num_processes) != 1 || num_processes <= 0 || num_processes > MAX_PROCESSES) {
        printf("Invalid number of processes. Setting to default 5.\n");
        num_processes = 5; 
        
        while (getchar() != '\n'); 
    }

    srand(time(NULL));
    printf("\n--- Generating Random Processes ---\n");
    printf("PID | Arrival | CPU Burst | I/O Request | I/O Burst | Priority\n");
    printf("----|---------|-----------|-------------|-----------|---------\n");

    for (int i = 0; i < num_processes; i++) {
        original_processes[i].pid = i + 1;
        original_processes[i].arrival_time = rand() % 20;
        original_processes[i].cpu_burst_time_initial = (rand() % 15) + 1;

        original_processes[i].io_burst_time = (rand() % 10) + 1;
        if(original_processes[i].io_burst_time==0)
            original_processes[i].io_request_time=0;
        else
            original_processes[i].io_request_time = (rand() % original_processes[i].cpu_burst_time_initial) + 1;

        original_processes[i].priority = rand() % 10; 
        printf("%3d | %7d | %9d | %10d | %9d | %8d\n",
               original_processes[i].pid, original_processes[i].arrival_time,
               original_processes[i].cpu_burst_time_initial, original_processes[i].io_request_time,
               original_processes[i].io_burst_time, original_processes[i].priority);
    }
}

void reset_processes_for_simulation() {
    for (int i = 0; i < num_processes; i++) {
        processes[i] = original_processes[i]; 
        processes[i].remaining_cpu_total = processes[i].cpu_burst_time_initial;
        processes[i].cpu_done_current_segment = 0;
        processes[i].remaining_cpu_after_io = 0;
        processes[i].start_time = -1;
        processes[i].completion_time = 0;
        processes[i].waiting_time = 0;
        processes[i].turnaround_time = 0;
        processes[i].response_time = -1;
        processes[i].state = 0; 
        processes[i].io_complete_at_time = 0;
        processes[i].has_started_execution = 0;
        processes[i].last_active_time = processes[i].arrival_time; 
        processes[i].current_quantum_slice = 0;
    }
    sort_processes_by_arrival(processes, num_processes);

    ready_queue_size = 0;
    waiting_queue_size = 0;
    gantt_idx = 0;
}

void Config() {
    printf("\n--- System Configuration ---\n");
    printf(" - Max Processes Allowed: %d\n", MAX_PROCESSES);
    printf(" - Time Quantum (for RR): %d\n", TIME_QUANTUM);

    ready_queue_size = 0;
    waiting_queue_size = 0;
    printf(" - Ready Queue and Waiting Queue initialized.\n");
    printf("   - Ready Queue Type: Array-based FIFO (behavior modified by schedulers)\n");
    printf("   - Waiting Queue Type: Array-based FIFO\n");
    printf("   - Max Queue Capacity: %d processes each\n", MAX_PROCESSES);
}

void add_gantt_entry(int pid, int start, int end) {

    if (gantt_idx > 0 && gantt_chart[gantt_idx-1].pid == pid && gantt_chart[gantt_idx-1].end == start) {
        gantt_chart[gantt_idx-1].end = end;
    } else if (gantt_idx < MAX_PROCESSES * 20) {
        gantt_chart[gantt_idx].pid = pid;
        gantt_chart[gantt_idx].start = start;
        gantt_chart[gantt_idx].end = end;
        gantt_idx++;
    }
}

void Evaluation(const char* algo_name) {
    float total_waiting_time = 0;
    float total_turnaround_time = 0;
    int completed_count = 0;

    printf("\n--- Evaluation for %s ---\n", algo_name);

    // Print Gantt Chart
    printf("Gantt Chart:\n|");
    for (int i = 0; i < gantt_idx; i++) {
        if (gantt_chart[i].start < gantt_chart[i].end) { // Only print if duration > 0
             printf(" P%d (%d-%d) |", gantt_chart[i].pid, gantt_chart[i].start, gantt_chart[i].end);
        }
    }
    printf("\n");

    for (int i = 0; i < num_processes; i++) {

        Process p_eval;
        int found = 0;
        for(int j=0; j<num_processes; ++j) {
            if(processes[j].pid == original_processes[i].pid) { 
                p_eval = processes[j];
                found = 1;
                break;
            }
        }
        if (!found) continue;

        if (p_eval.state == 4) {
            p_eval.turnaround_time = p_eval.completion_time - p_eval.arrival_time;
            p_eval.waiting_time = p_eval.turnaround_time - p_eval.cpu_burst_time_initial;
            if (p_eval.waiting_time < 0) p_eval.waiting_time = 0;

            total_waiting_time += p_eval.waiting_time;
            total_turnaround_time += p_eval.turnaround_time;
            completed_count++;
        }
    }

    if (completed_count > 0) {
        printf("Average Waiting Time: %.2f\n", total_waiting_time / completed_count);     
        printf("Average Turnaround Time: %.2f\n", total_turnaround_time / completed_count); 
    } else {
        printf("\nNo processes were completed to evaluate.\n");
    }
}

int simulate_process_tick(Process* p, int current_time) {
    p->remaining_cpu_total--;
    p->cpu_done_current_segment++;
    
    int is_first_cpu_segment = (p->remaining_cpu_after_io == 0);
    if (is_first_cpu_segment &&
        p->io_request_time > 0 &&
        p->io_burst_time > 0 &&
        p->cpu_done_current_segment == p->io_request_time &&
        p->io_request_time < p->cpu_burst_time_initial) {
        
        p->state = 3;
        p->io_complete_at_time = current_time + 1 + p->io_burst_time;
        p->remaining_cpu_after_io = p->cpu_burst_time_initial - p->io_request_time;
        p->remaining_cpu_total = p->remaining_cpu_after_io; 
        
        if (waiting_queue_size < MAX_PROCESSES) {
            waiting_queue[waiting_queue_size++] = *p;
        }
        return 1;
    }
    
    if (p->remaining_cpu_total == 0) {
        p->state = 4;
        p->completion_time = current_time + 1;
        return 2;
    }
    return 0;
}

void run_scheduler_generic(const char* algo_name, 
                           Process (*get_next_process_from_ready_queue)(),
                           Process (*peek_next_process_in_ready_queue)()) {
    reset_processes_for_simulation();
    printf("\n--- Running %s Scheduler ---\n", algo_name);

    int current_time = 0;
    int completed_count = 0;
    Process* running_process = NULL;
    int current_process_start_cpu_time = 0;


    while(completed_count < num_processes) {
        for(int i=0; i<num_processes; ++i) {
            if(processes[i].state == 0 && processes[i].arrival_time <= current_time) {
                processes[i].state = 1;
                processes[i].last_active_time = current_time;
                add_to_ready_queue(processes[i]);
            }
        }

        for(int i=0; i<waiting_queue_size; ++i) {
            if(waiting_queue[i].io_complete_at_time <= current_time) {
                Process p_io_done = waiting_queue[i];

                for(int j=i; j<waiting_queue_size-1; ++j) waiting_queue[j] = waiting_queue[j+1];
                waiting_queue_size--;
                i--;

                for(int k=0; k<num_processes; ++k) {
                    if(processes[k].pid == p_io_done.pid) {
                        processes[k].state = 1;
                        processes[k].last_active_time = current_time;
                        processes[k].cpu_done_current_segment = 0;
                        add_to_ready_queue(processes[k]);
                        break;
                    }
                }
            }
        }
        
        if (running_process != NULL && 
            ( (strcmp(algo_name, "Preemptive SJF") == 0 && peek_next_process_in_ready_queue != NULL) ||
              (strcmp(algo_name, "Preemptive Priority") == 0 && peek_next_process_in_ready_queue != NULL) )
           ) {
            Process potential_preemptor = peek_next_process_in_ready_queue();
            if (potential_preemptor.pid != -1) {
                int should_preempt = 0;
                if (strcmp(algo_name, "Preemptive SJF") == 0 && 
                    potential_preemptor.remaining_cpu_total < running_process->remaining_cpu_total) {
                    should_preempt = 1;
                } else if (strcmp(algo_name, "Preemptive Priority") == 0 &&
                           potential_preemptor.priority < running_process->priority) {
                    should_preempt = 1;
                }

                if (should_preempt) {
                    add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time);
                    running_process->state = 1;
                    running_process->last_active_time = current_time;
                    running_process->current_quantum_slice = 0;
                    add_to_ready_queue(*running_process);
                    running_process = NULL;
                }
            }
        }

        if (running_process == NULL && ready_queue_size > 0) {
            Process next_p_q = get_next_process_from_ready_queue();
            if (next_p_q.pid != -1) {
                for(int i=0; i<num_processes; ++i) {
                    if(processes[i].pid == next_p_q.pid && processes[i].state != 4) {
                        running_process = &processes[i];
                        break;
                    }
                }
                if (running_process) {
                    running_process->state = 2;
                    if (!running_process->has_started_execution) {
                        running_process->start_time = current_time;
                        running_process->response_time = current_time - running_process->arrival_time;
                        running_process->has_started_execution = 1;
                    }
                    running_process->waiting_time += current_time - running_process->last_active_time;
                    running_process->current_quantum_slice = 0;
                    current_process_start_cpu_time = current_time;
                }
            }
        }

        if (running_process != NULL) {
            int tick_result = simulate_process_tick(running_process, current_time);
            running_process->current_quantum_slice++;

            if (tick_result == 1) {
                add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time + 1);
                running_process = NULL;
            } else if (tick_result == 2) {
                add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time + 1);
                completed_count++;
                running_process = NULL;
            } else {
                if (strcmp(algo_name, "Round Robin") == 0 && running_process->current_quantum_slice >= TIME_QUANTUM) {
                    add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time + 1);
                    running_process->state = 1;
                    running_process->last_active_time = current_time + 1;
                    add_to_ready_queue(*running_process);
                    running_process = NULL;
                }
            }
        }

        current_time++;
        if (current_time > 5000 && completed_count < num_processes) {
            printf("Simulation for %s possibly stuck. Time: %d, Completed: %d/%d\n", algo_name, current_time, completed_count, num_processes);
            break; 
        }
        if (completed_count == num_processes && running_process == NULL && ready_queue_size == 0 && waiting_queue_size == 0) break;
        if (completed_count < num_processes && running_process == NULL && ready_queue_size == 0 && waiting_queue_size == 0) {
            int next_event_time = INT_MAX;
            for(int i=0; i<num_processes; ++i) if(processes[i].state == 0 && processes[i].arrival_time < next_event_time) next_event_time = processes[i].arrival_time;
            for(int i=0; i<waiting_queue_size; ++i) if(waiting_queue[i].io_complete_at_time < next_event_time) next_event_time = waiting_queue[i].io_complete_at_time;
            
            if (next_event_time != INT_MAX && next_event_time > current_time) {
                add_gantt_entry(0, current_time, next_event_time);
                current_time = next_event_time;
                continue;
            } else if (next_event_time == INT_MAX && completed_count < num_processes) {
                break;
            }
        }
    }
    Evaluation(algo_name);
}

void Schedule_FCFS() {
    run_scheduler_generic("FCFS", get_from_ready_queue_fcfs, NULL);
}

void Schedule_SJF_NonPreemptive() {
    run_scheduler_generic("Non-Preemptive SJF", get_from_ready_queue_sjf, NULL);
}

void Schedule_SJF_Preemptive() {
    run_scheduler_generic("Preemptive SJF", get_from_ready_queue_sjf, peek_ready_queue_sjf);
}

void Schedule_Priority_NonPreemptive() {
    run_scheduler_generic("Non-Preemptive Priority", get_from_ready_queue_priority, NULL);
}

void Schedule_Priority_Preemptive() {
    run_scheduler_generic("Preemptive Priority", get_from_ready_queue_priority, peek_ready_queue_priority);
}

void Schedule_RR() {
    run_scheduler_generic("Round Robin", get_from_ready_queue_rr, NULL); // RR preemption is handled differently (timer based)
}


int main() {
    printf("Welcome to the CPU scheduling program!\n");
    printf("You can compare various algorithms in here.\n");

    int choice;
    
    while(1){
        display_menu();
        scanf("%d", &choice);

        switch(choice){
            case 1:
                Create_Process();
                Config();
                break;
            case 2:
                Schedule_FCFS();
                break;
            case 3:
                Schedule_SJF_NonPreemptive();
                break; 
            case 4:
                Schedule_SJF_Preemptive();
                break;
            case 5:
                Schedule_Priority_NonPreemptive();
                break;
            case 6:
                Schedule_Priority_Preemptive();
                break;
            case 7:
                Schedule_RR();
                break;
            case 0:
                printf("Exit the program. Thank you!\n");
                exit(0);
            default:
                printf("Wrong select. Please write correctly.\n");
        }

        printf("\nPress Enter to continue\n");
        getchar();
        getchar();
    }                   

    return 0;
}