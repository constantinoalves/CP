#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    int iterations;  // number of operations
    pthread_mutex_t mutex_it;
    pthread_mutex_t *mutex;  //Mutex para cada cuenta
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
        usleep(args->delay);
        if (args->bank->iterations == 0){
            pthread_mutex_unlock(&args->bank->mutex_it);
            break;
        }
        else{
        args->bank->iterations--;
        usleep(args->delay);
        pthread_mutex_unlock(&args->bank->mutex_it);
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;

        printf("Thread %d depositing %d on account %d\n",
               args->thread_num, amount, account);

        pthread_mutex_lock(&args->bank->mutex[account]);

        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
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
        usleep(args->delay);
        if (args->bank->iterations == 0){
            pthread_mutex_unlock(&args->bank->mutex_it);
            break;
        }
        else{
            args->bank->iterations--;
            usleep(args->delay);
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
            pthread_mutex_unlock(&args->bank->mutex[account2]);
            pthread_mutex_unlock(&args->bank->mutex[account1]);
        }
    }
    return NULL;
}


//Totales



// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank)
{
    int i;
    struct thread_info *threads;

    printf("creating %d threads\n", (2*opt.num_threads));
    threads = malloc((2*sizeof(struct thread_info) * opt.num_threads) + 1);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }
    bank->iterations = opt.iterations;
    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;

        if (0 != pthread_create(&threads[i].id, NULL, deposit, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    for (i = 0; i < opt.num_threads; i++) pthread_join(threads[i].id,NULL);

    bank->iterations = opt.iterations;

    // threads para las tranferencias
    for (i = opt.num_threads; i < (2*opt.num_threads); i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;
        if (0 != pthread_create(&threads[i].id, NULL, transferencia, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }
    return threads;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_deposits=0, bank_total=0, total_transferred=0;
    //Depositos
    printf("\nNet deposits by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);

     //Transferencias
    printf("\nNet transfers by thread \n");

    for(int i = num_threads; i < (2*num_threads); i++){
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_transferred += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_transferred);

    //Balances 
    printf("\nAccount balance\n");

    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);

}

// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct bank *bank, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = opt.num_threads; i < (2*opt.num_threads); i++)
        pthread_join(threads[i].id, NULL);
    print_balances(bank, threads, opt.num_threads);

    for (int i = 0; i < (2*opt.num_threads); i++)
        free(threads[i].args);

    free(threads);
    free(bank->accounts);
    free(bank->mutex);
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank-> mutex = malloc(bank->num_accounts * sizeof(pthread_mutex_t));
    pthread_mutex_init(&bank->mutex_it, NULL);


    for(int i=0; i < bank->num_accounts; i++)
        bank->accounts[i] = 0;
    for(int i=0; i< bank->num_accounts; i++)
        pthread_mutex_init(&bank->mutex[i], NULL);
}

int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct thread_info *thrs;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 10;
    opt.delay        = 10;
    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

    thrs = start_threads(opt, &bank);
    wait(opt, &bank, thrs);

    return 0;
}
