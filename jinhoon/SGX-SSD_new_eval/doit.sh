g++ -o mapping_app mapping_app.cpp
g++ -o request_pid_app request_pid_app.cpp
g++ -o input_app input_app.cpp
gcc -pthread -o eval_performance_final eval_performance_final.c -lrt
gcc -pthread -o baseline baseline.c -lrt
