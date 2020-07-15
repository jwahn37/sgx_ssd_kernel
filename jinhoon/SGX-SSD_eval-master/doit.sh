g++ -pthread -o mapping_app mapping_app.cpp -lrt
g++ -pthread -o request_pid_app request_pid_app.cpp -lrt
g++ -o input_app input_app.cpp
gcc -pthread -o eval_performance_final eval_performance_final.c -lrt
