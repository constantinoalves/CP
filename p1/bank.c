#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
    int active; //Determina cuando debe parar el supervisor
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    int iterations;          // number of operations
    pthread_mutex_t mutex_it;
    pthread_mutex_t *mutex;  //Mutex para cada cuenta
    pthread_mutex_t mutex_act; //Mutex para el supervisor
};

struct args {
    int          thread_num;  // application defined thread #
    int          delay;       // delay between operations
    int          net_total;   // total amount deposited by this thread
    struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

// Threads run on this function
void *deposit(void *ptr)
{
    struct args *args =  ptr;
    int amount, account, balance;

    while(1){
        pthread_mutex_lock(&args->bank->mutex_it);
        if(args->delay) usleep(args->delay);
        if (args->bank->iterations == 0){
            pthread_mutex_unlock(&args->bank->mutex_it);
            break;
        }
        else{
        args->bank->iterations--;
        if(args->delay) usleep(args->delay);
        pthread_mutex_unlock(&args->bank->mutex_it);
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;

        printf("Thread %d depositing %d on account %d\n",
               args->thread_num, amount, account);

        pthread_mutex_lock(&args->bank->mutex[account]);
        
        //Inicio sección crítica
        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
        //Final sección crítica
        
        pthread_mutex_unlock(&args->bank->mutex[account]);
        }
    }
    return NULL;
}

//Transferencias
void *transferencia(void *ptr){

    struct args *args = ptr;
    int amount, account1, account2, balance1, balance2;

    while(1){
        pthread_mutex_lock(&args->bank->mutex_it);
        if(args->delay) usleep(args->delay);
        if (args->bank->iterations == 0){
            pthread_mutex_unlock(&args->bank->mutex_it);
            break;
        }
        else{
            args->bank->iterations--;
            if(args->delay) usleep(args->delay);
            pthread_mutex_unlock(&args->bank->mutex_it);
            account1 = rand() % args->bank->num_accounts;
            do{
                account2 = rand() % args -> bank -> num_accounts;
            }while (account2 == account1); //Do while que corrige la posible transferencia de una cuenta a ella misma
            
            
            if(&args->bank->mutex[account1] < &args->bank->mutex[account2]){ // Reserva ordenada
                pthread_mutex_lock(&args->bank->mutex[account1]);
                pthread_mutex_lock(&args->bank->mutex[account2]);
            }else{
                pthread_mutex_lock(&args->bank->mutex[account2]);
                pthread_mutex_lock(&args->bank->mutex[account1]);
            }
            
            //Inicio sección crítica
            if(args->bank->accounts[account1] > 0){
                amount = rand() % args->bank->accounts[account1]; //Cantidad transferida random entre 0 y el total de account1
            }else{
                amount = 0;
            }

            printf("Thread %d depositing %d from account %d to account %d\n",
                args->thread_num, amount, account1, account2);

            balance1 = args->bank->accounts[account1];
            balance2 = args->bank->accounts[account2];
            if(args->delay) usleep(args->delay); // Force a context switch

            balance1 -= amount; //Restamos a la primera cuenta lo transferido (localmente)
            balance2 += amount; //Sumamos a la segunda cuenta lo transferido (localmente)
            if(args->delay) usleep(args->delay); // Force a context switch

            args->bank->accounts[account1] = balance1;
            args->bank->accounts[account2] = balance2;
            if(args->delay) usleep(args->delay); // Force a context switch

            args->net_total += amount; //Sumamos al thread que envía el dinero lo ingresado
            //Final sección crítica
            
            pthread_mutex_unlock(&args->bank->mutex[account2]);
            pthread_mutex_unlock(&args->bank->mutex[account1]);
        }
    }
    return NULL;
}


//Totales
void *supervisor(void *ptr){
    int total_acc;
    struct args *args = ptr;
    pthread_mutex_lock(&args->bank->mutex_act);
    usleep(args->delay);
    while(args->bank->active){
        usleep(args->delay);
        pthread_mutex_unlock(&args->bank->mutex_act);
        total_acc= 0;
        for (int i = 0; i < args->bank->num_accounts; i++)
            pthread_mutex_lock(&args->bank->mutex[i]);
        if(args->delay) usleep(args->delay);
        for (int i = 0; i < args->bank->num_accounts; i++)
            total_acc += args->bank->accounts[i];

        printf("Suma del monto de las cuentas: %d\n", total_acc);
        for (int i = 0; i < args->bank->num_accounts; i++)
            pthread_mutex_unlock(&args->bank->mutex[i]);

    }
    pthread_mutex_unlock(&args->bank->mutex_act);
return NULL;
}

struct thread_info *supervisar(struct options opt, struct bank *bank)
{
    struct thread_info *threads;

    printf("creating supervisor thread\n");
    threads = malloc(sizeof(struct thread_info));

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }
        threads[0].args = malloc(sizeof(struct args));

        threads[0].args -> thread_num = 0;
        threads[0].args -> net_total  = 0;
        threads[0].args -> bank       = bank;
        threads[0].args -> delay      = opt.delay;

        if (0 != pthread_create(&threads[0].id, NULL, supervisor, threads[0].args)) {
            printf("Could not create supervisor thread");
            exit(1);
        }
return threads;
}


// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank, void *funcion)
{
    int i;
    struct thread_info *threads;
    
    bank->iterations = opt.iterations;
    printf("creating %d threads\n", (2*opt.num_threads));
    threads = malloc((sizeof(struct thread_info) * opt.num_threads));

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }
    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;

        if (0 != pthread_create(&threads[i].id, NULL, funcion, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }
    return threads; 
}


//Depositos
void print_deposits(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_deposits=0;
    printf("\nNet deposits by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n\n", total_deposits);

}


//Transferencias
void print_transfers(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_transferred=0;
    printf("\nNet transfers by thread \n");

    for(int i = 0; i < num_threads; i++){
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_transferred += thrs[i].args->net_total;
    }
    printf("Total: %d\n\n", total_transferred);

}

// Print the final balances of accounts and threads
void print_accounts(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int bank_total=0;
    printf("\nAccount balance\n");

    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);

}


void wait(struct options opt, struct bank *bank, struct thread_info *threads, void (*funcion)(struct bank *, struct thread_info *, int)) {

    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);
    funcion(bank, threads, opt.num_threads);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->active = 1;
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank-> mutex = malloc(bank->num_accounts * sizeof(pthread_mutex_t));
    pthread_mutex_init(&bank->mutex_it, NULL);
    pthread_mutex_init(&bank->mutex_act, NULL);

    for(int i=0; i < bank->num_accounts; i++)
        bank->accounts[i] = 0;
    for(int i=0; i< bank->num_accounts; i++)
        pthread_mutex_init(&bank->mutex[i], NULL);
}

int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct thread_info *thrs, *thr_su;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 10;
    opt.delay        = 10;
    read_options(argc, argv, &opt);
    //Iniciamos cuentas con valores aleatorios
    init_accounts(&bank, opt.num_accounts);

    thrs = start_threads(opt, &bank, deposit);
    wait(opt, &bank, thrs, print_deposits);
    thrs = start_threads(opt, &bank, transferencia);
    thr_su = supervisar(opt, &bank);
    wait(opt, &bank, thrs, print_transfers);
    pthread_mutex_lock(&bank.mutex_act);
    usleep(opt.delay);
    bank.active= 0;
    pthread_mutex_lock(&bank.mutex_act);
    pthread_join(thr_su[0].id, NULL);
    print_accounts(&bank, thrs, opt.num_threads);

    //Liberamos memoria
    free(thr_su->args);
    free(thr_su);
    free(bank.accounts);
    free(bank.mutex);
    return 0;
}
