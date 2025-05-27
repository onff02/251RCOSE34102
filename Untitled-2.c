#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <string.h>

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

    // Dynamic fields for simulation
    int remaining_cpu_total;
    int cpu_done_current_segment;   // process가 실행되고 있을 때 
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
    
    // For heap ordering in different contexts
    int queue_entry_time; // When this process entered the current queue
} Process;

Process processes[MAX_PROCESSES];
Process original_processes[MAX_PROCESSES];
int num_processes;

// Heap structures for different scheduling algorithms
typedef struct {
    Process* heap[MAX_PROCESSES];
    int size;
    int (*compare)(Process* a, Process* b); // Comparison function for heap ordering
} ProcessHeap;

// Queue structures - will be initialized in Config()
ProcessHeap ready_queue;
ProcessHeap waiting_queue;

// Gantt chart
typedef struct {
    int pid;
    int start;
    int end;
} GanttEntry;
GanttEntry gantt_chart[MAX_PROCESSES * 20];
int gantt_idx = 0;

// Current scheduling mode for ready queue comparison
enum SchedulingMode {
    FCFS_MODE,
    SJF_MODE,
    PRIORITY_MODE,
    RR_MODE
} current_scheduling_mode;

// Comparison functions for different scheduling algorithms
int compare_fcfs(Process* a, Process* b) {
    if (a->arrival_time != b->arrival_time) {
        return a->arrival_time - b->arrival_time; // Earlier arrival has higher priority
    }
    return a->pid - b->pid; // Tie-break by PID
}

int compare_sjf(Process* a, Process* b) {
    if (a->remaining_cpu_total != b->remaining_cpu_total) {
        return a->remaining_cpu_total - b->remaining_cpu_total; // Shorter job has higher priority
    }
    if (a->arrival_time != b->arrival_time) {
        return a->arrival_time - b->arrival_time; // Earlier arrival breaks tie
    }
    return a->pid - b->pid;
}

int compare_priority(Process* a, Process* b) {
    if (a->priority != b->priority) {
        return a->priority - b->priority; // Lower number = higher priority
    }
    if (a->arrival_time != b->arrival_time) {
        return a->arrival_time - b->arrival_time; // Earlier arrival breaks tie
    }
    return a->pid - b->pid;
}

int compare_rr(Process* a, Process* b) {
    if (a->queue_entry_time != b->queue_entry_time) {
        return a->queue_entry_time - b->queue_entry_time; // FIFO order for RR
    }
    return a->pid - b->pid;
}

int compare_io_completion(Process* a, Process* b) {
    if (a->io_complete_at_time != b->io_complete_at_time) {
        return a->io_complete_at_time - b->io_complete_at_time; // Earlier completion first
    }
    return a->pid - b->pid;
}

// Heap utility functions
void heap_init(ProcessHeap* heap, int (*compare_func)(Process* a, Process* b)) {
    heap->size = 0;
    heap->compare = compare_func;
}

void heap_swap(ProcessHeap* heap, int i, int j) {
    Process* temp = heap->heap[i];
    heap->heap[i] = heap->heap[j];
    heap->heap[j] = temp;
}

// algorithm의 우선순위(FCFS-earlier arrival time)에 맞게 heap의 맨 끝부터 확인하며 내부 정렬 
void heap_heapify_up(ProcessHeap* heap, int index) {
    if (index == 0) return;
    
    int parent = (index - 1) / 2;
    if (heap->compare(heap->heap[index], heap->heap[parent]) < 0) {
        heap_swap(heap, index, parent);
        heap_heapify_up(heap, parent);
    }
}

//가장 큰 수가 parent에 있을 때 아래로 내려가면서 작은 수로 정렬하는 방법
void heap_heapify_down(ProcessHeap* heap, int index) {
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    int smallest = index;

    if (left < heap->size && heap->compare(heap->heap[left], heap->heap[smallest]) < 0) {
        smallest = left;
    }
    if (right < heap->size && heap->compare(heap->heap[right], heap->heap[smallest]) < 0) {
        smallest = right;
    }

    if (smallest != index) {
        heap_swap(heap, index, smallest);
        heap_heapify_down(heap, smallest);
    }
}

// heap 맨 끝에 process 삽입 후 정렬
void heap_insert(ProcessHeap* heap, Process* process) {
    if (heap->size >= MAX_PROCESSES) return;
    
    heap->heap[heap->size] = process;
    heap_heapify_up(heap, heap->size);
    heap->size++;
}

//heap의 맨 끝 process를 root로 올리고 정렬하여 min 추출
Process* heap_extract_min(ProcessHeap* heap) {
    if (heap->size == 0) return NULL;
    
    Process* min = heap->heap[0];
    heap->heap[0] = heap->heap[heap->size - 1];
    heap->size--;
    
    if (heap->size > 0) {
        heap_heapify_down(heap, 0);
    }
    
    return min;
}

// Remove specific process from heap (for preemption)
int heap_remove_process(ProcessHeap* heap, int pid) {
    int index = -1;
    for (int i = 0; i < heap->size; i++) {
        if (heap->heap[i]->pid == pid) {
            index = i;
            break;
        }
    }
    
    if (index == -1) return 0; // Process not found
    
    heap->heap[index] = heap->heap[heap->size - 1];
    heap->size--;
    
    if (heap->size > 0 && index < heap->size) {
        // Try both up and down heapify
        int parent = (index - 1) / 2;
        if (index > 0 && heap->compare(heap->heap[index], heap->heap[parent]) < 0) {
            heap_heapify_up(heap, index);
        } else {
            heap_heapify_down(heap, index);
        }
    }
    
    return 1; // Success
}

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

void display_menu() {
    printf("--------------------------------------\n");
    printf("         CPU SCHEDULING SIMULATOR \n\n");
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
        
        if (original_processes[i].io_burst_time == 0)
            original_processes[i].io_request_time = 0;
        else
            original_processes[i].io_request_time = (rand() % original_processes[i].cpu_burst_time_initial) + 1;

        original_processes[i].priority = rand() % 10;
        printf("%3d | %7d | %9d | %10d | %9d | %8d\n",
               original_processes[i].pid, original_processes[i].arrival_time,
               original_processes[i].cpu_burst_time_initial, original_processes[i].io_request_time,
               original_processes[i].io_burst_time, original_processes[i].priority);
    }
    
    printf("\n--- Processes Created Successfully ---\n");
}

void Config() {
    printf("\n--- System Configuration ---\n");

    printf("\nReady Queue Configuration:\n");
    heap_init(&ready_queue, compare_fcfs); // Default initialization, will be reconfigured per algorithm
     
    printf("\nWaiting Queue Configuration:\n");
    heap_init(&waiting_queue, compare_io_completion);
    
    printf("\n--- Configuration Complete ---\n");
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
        processes[i].queue_entry_time = 0;
    }
    sort_processes_by_arrival(processes, num_processes);

    // Reset queues (they are already configured in Config())
    ready_queue.size = 0;
    waiting_queue.size = 0;
    gantt_idx = 0;
}

// gantt chart 작성
void add_gantt_entry(int pid, int start, int end) {
    // 이전과 같은 구간을 연속적으로 표현해야 할 때
    if (gantt_idx > 0 && gantt_chart[gantt_idx-1].pid == pid && gantt_chart[gantt_idx-1].end == start) {
        gantt_chart[gantt_idx-1].end = end;
    } 
    // 새로운 pid 기록
    else if (gantt_idx < MAX_PROCESSES * 20) {
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
        if (gantt_chart[i].start < gantt_chart[i].end) {
            printf(" P%d (%d-%d) |", gantt_chart[i].pid, gantt_chart[i].start, gantt_chart[i].end);
        }
    }
    printf("\n");

    printf("\nProcess Details:\n");
    printf("PID | Arrival | Completion | Turnaround | Waiting | Response\n");
    printf("----|---------|------------|------------|---------|----------\n");

    for (int i = 0; i < num_processes; i++) {
        Process p_eval;
        int found = 0;
        for (int j = 0; j < num_processes; ++j) {
            if (processes[j].pid == original_processes[i].pid) {
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

            printf("%3d | %7d | %10d | %10d | %7d | %8d\n",
                   p_eval.pid, p_eval.arrival_time, p_eval.completion_time,
                   p_eval.turnaround_time, p_eval.waiting_time, p_eval.response_time);

            total_waiting_time += p_eval.waiting_time;
            total_turnaround_time += p_eval.turnaround_time;
            completed_count++;
        }
    }

    if (completed_count > 0) {
        printf("\n--- Performance Metrics ---\n");
        printf("Average Waiting Time: %.2f\n", total_waiting_time / completed_count);
        printf("Average Turnaround Time: %.2f\n", total_turnaround_time / completed_count);
    } else {
        printf("\nNo processes were completed to evaluate.\n");
    }
}

int simulate_process_tick(Process* p, int current_time) {
    // 1) cpu usuage update
    p->remaining_cpu_total--;
    p->cpu_done_current_segment++;

    // 2. Check if reach to I/O request time
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

        heap_insert(&waiting_queue, p);
        return 1;
    }

    // Check if process's burst time is done
    if (p->remaining_cpu_total == 0) {
        p->state = 4;
        p->completion_time = current_time + 1;
        return 2;
    }
    return 0;
}

void run_scheduler_generic(const char* algo_name, enum SchedulingMode mode) {
    reset_processes_for_simulation();
    current_scheduling_mode = mode;
    
    // Reconfigure ready queue with appropriate comparison function
    switch (mode) {
        case FCFS_MODE:
            ready_queue.compare = compare_fcfs;
            break;
        case SJF_MODE:
            ready_queue.compare = compare_sjf;
            break;
        case PRIORITY_MODE:
            ready_queue.compare = compare_priority;
            break;
        case RR_MODE:
            ready_queue.compare = compare_rr;
            break;
    }

    printf("\n--- Running %s Scheduler ---\n", algo_name);

    int current_time = 0;
    int completed_count = 0;
    Process* running_process = NULL;
    int current_process_start_cpu_time = 0;

    while (completed_count < num_processes) {
        // 1. Add newly arrived processes to ready queue
        for (int i = 0; i < num_processes; ++i) {
            if (processes[i].state == 0 && processes[i].arrival_time <= current_time) {
                processes[i].state = 1;
                processes[i].last_active_time = current_time;
                processes[i].queue_entry_time = current_time;
                heap_insert(&ready_queue, &processes[i]);
            }
        }

        // 2. Check for I/O completions using waiting queue
        while (waiting_queue.size != 0) {
            Process* p_waiting = waiting_queue.heap[0];
            // I/O가 끝난 상태라면
            if (p_waiting->io_complete_at_time <= current_time) {
                // heap에서 꺼내서
                Process* p = heap_extract_min(&waiting_queue);
                
                // Update the actual process and add to ready queue
                for (int k = 0; k < num_processes; ++k) {
                    if (processes[k].pid == p->pid) {
                        processes[k].state = 1;
                        processes[k].last_active_time = current_time;
                        processes[k].cpu_done_current_segment = 0;
                        processes[k].queue_entry_time = current_time;
                        heap_insert(&ready_queue, &processes[k]);
                        break;
                    }
                }
            } else {
                break;
            }
        }

        // 3. Preemption logic for preemptive algorithms
        if (running_process != NULL && ready_queue.size != 0) {
            Process* potential_preemptor = ready_queue.heap[0];
            int should_preempt = 0;

            if (strstr(algo_name, "Preemptive SJF") &&
                potential_preemptor->remaining_cpu_total < running_process->remaining_cpu_total) {
                should_preempt = 1;
            } else if (strstr(algo_name, "Preemptive Priority") &&
                       potential_preemptor->priority < running_process->priority) {
                should_preempt = 1;
            }

            if (should_preempt) {
                add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time);
                running_process->state = 1;
                running_process->last_active_time = current_time;
                running_process->current_quantum_slice = 0;
                running_process->queue_entry_time = current_time;
                heap_insert(&ready_queue, running_process);
                running_process = NULL;
            }
        }

        // 4. Dispatch if CPU is idle (using configured ready queue)
        if (running_process == NULL && ready_queue.size != 0) {
            Process* next_p = heap_extract_min(&ready_queue);
            if (next_p) {
                running_process = next_p;
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

        // 5. Simulate CPU execution for one tick
        if (running_process != NULL) {
            int tick_result = simulate_process_tick(running_process, current_time);
            running_process->current_quantum_slice++;

            if (tick_result == 1) { // Went to I/O (added to waiting queue)
                add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time + 1);
                running_process = NULL;
            } else if (tick_result == 2) { // Fully completed
                add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time + 1);
                completed_count++;
                running_process = NULL;
            } else { // Continues running
                // RR Preemption
                if (mode == RR_MODE && running_process->current_quantum_slice >= TIME_QUANTUM) {
                    add_gantt_entry(running_process->pid, current_process_start_cpu_time, current_time + 1);
                    running_process->state = 1;
                    running_process->last_active_time = current_time + 1;
                    running_process->queue_entry_time = current_time + 1;
                    heap_insert(&ready_queue, running_process);
                    running_process = NULL;
                }
            }
        }

        current_time++;
        // 실행 안되는 문제 발생하여 강제 종료
        if (current_time > 5000 && completed_count < num_processes) {
            printf("Simulation for %s possibly stuck. Time: %d, Completed: %d/%d\n", 
                   algo_name, current_time, completed_count, num_processes);
            break;
        }
        // 모든 실행 완료
        if (completed_count == num_processes && running_process == NULL && 
            ready_queue.size == 0 && waiting_queue.size == 0) break;
        
        // 아직 process 도착 전인데, ready queue 내 모든 process 실행 완료
        if (completed_count < num_processes && running_process == NULL && 
            ready_queue.size == 0) {
            // Advance time to next event if CPU idle
            int next_event_time = INT_MAX;
            for (int i = 0; i < num_processes; ++i) {
                if (processes[i].state == 0 && processes[i].arrival_time < next_event_time) {
                    next_event_time = processes[i].arrival_time;
                }
            }
            // I/O waiting 존재 시
            if (waiting_queue.size != 0) {
                Process* next_io = waiting_queue.size == 0 ? NULL : waiting_queue.heap[0];
                if (next_io->io_complete_at_time < next_event_time) {
                    next_event_time = next_io->io_complete_at_time;
                }
            }
            // 아직 process 도착 전
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

// Specific Schedulers
void Schedule_FCFS() {
    run_scheduler_generic("FCFS", FCFS_MODE);
}

void Schedule_SJF_NonPreemptive() {
    run_scheduler_generic("Non-Preemptive SJF", SJF_MODE);
}

void Schedule_SJF_Preemptive() {
    run_scheduler_generic("Preemptive SJF", SJF_MODE);
}

void Schedule_Priority_NonPreemptive() {
    run_scheduler_generic("Non-Preemptive Priority", PRIORITY_MODE);
}

void Schedule_Priority_Preemptive() {
    run_scheduler_generic("Preemptive Priority", PRIORITY_MODE);
}

void Schedule_RR() {
    run_scheduler_generic("Round Robin", RR_MODE);
}

int main() {
    printf("\nWelcome to the CPU Scheduling Simulator!\n");

    int choice;

    while (1) {
        display_menu();
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                Create_Process();
                Config();
                break;
            case 2:
                if (num_processes == 0) {
                    printf("Please create processes first (Option 1)\n");
                    break;
                }
                Schedule_FCFS();
                break;
            case 3:
                if (num_processes == 0) {
                    printf("Please create processes first (Option 1)\n");
                    break;
                }
                Schedule_SJF_NonPreemptive();
                break;
            case 4:
                if (num_processes == 0) {
                    printf("Please create processes first (Option 1)\n");
                    break;
                }
                Schedule_SJF_Preemptive();
                break;
            case 5:
                if (num_processes == 0) {
                    printf("Please create processes first (Option 1)\n");
                    break;
                }
                Schedule_Priority_NonPreemptive();
                break;
            case 6:
                if (num_processes == 0) {
                    printf("Please create processes first (Option 1)\n");
                    break;
                }
                Schedule_Priority_Preemptive();
                break;
            case 7:
                if (num_processes == 0) {
                    printf("Please create processes first (Option 1)\n");
                    break;
                }
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