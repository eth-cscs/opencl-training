typedef float real_t;
kernel void arrayset(global real_t* outputArray,
	                 real_t value) {
	//get global thread id for dimension 0
	const int id = get_global_id(0);
	outputArray[id] = value; 
}