#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <vector>
#include <string.h>
#include <fstream>
#include <cstring>

using namespace std;

// Funcion que agrega data tipo 'string' al final de archivo especifico
static void AppendLineToFile(string filepath, string line)
{	
    ofstream file; // Define variable file como un objeto de salida de archivo 'ofstream'
	
	/*El modo de agregar (ios::app) agrega nuevos datos al final del archivo en lugar de 
	sobrescribir los datos existentes. Si el archivo especificado no existe, se creará uno nuevo.*/
    file.open(filepath, ios::out | ios::app); //abre archivo en modo escritura y modo agregar

	// Si falla la apertura del archivo, lanza una excepción con el mensaje de error.
    if (file.fail())
        throw ios_base::failure(strerror(errno));

	// Establece las excepciones a lanzar en caso de error de lectura/escritura.
    file.exceptions(file.exceptions() | ios::failbit | ifstream::badbit);

	cout<<line<<endl; // Imprime la línea de texto en la consola.
    file<<line<<endl; // Agrega la línea de texto al archivo y luego lo cierra.

}

class buffer{
	public:
		/* Declaración de dos variables enteras para el contador y 
		el número máximo de elementos permitidos en el buffer */
		int count, max;
		/* Declaración de una variable booleana que 
		indica si la inserción de un elemento en el buffer fue exitosa o no */
		bool successful_insertion; 
		/* Declaración de un vector de cadenas de caracteres que 
		representa la lista de elementos del buffer */
		vector<string> elements_list; 
		
		buffer(){ // Constructor de la clase buffer que inicializa el contador en cero
			count = 0;
		}

		// Método que agrega un elemento al buffer recibiendo como parámetro una cadena de caracteres
		void addItem(string p){ 
			if (count < max){ // Si la cantidad de elementos en el buffer es menor al máximo permitido
	   		   elements_list.insert(elements_list.begin(),p); // Inserta el elemento en la 1ra posición del vector
			   count++; // Incrementa el contador
			   successful_insertion = true; // Indica que la inserción fue exitosa
			}
			else{ // Si la cantidad de elementos en el buffer es igual al máximo permitido
				cout << "Error de insercion!"<<endl; // Imprime un mensaje de error
				successful_insertion = false; // Indica que la inserción no fue exitosa
			}
		}

		string removeItem(){ // Método que remueve un elemento del buffer
			string prod = elements_list.back(); // Obtiene el último elemento del vector
			elements_list.pop_back(); // Lo elimina del vector
			count--; // Decrementa el contador
			return prod; // Retorna el elemento eliminado
		}
};

buffer p; // Inicializacion de buffer
int NP, NC, BC, NPP, NCC; // Variables que almacenarán los parámetros iniciales

pthread_mutex_t mutex_buffer; // Declaración del mutex para proteger el acceso al buffer en el hilo del productor
sem_t sem_full; // Declaración del semáforo para controlar el número de espacios ocupados en el buffer
sem_t sem_empty; // Declaración del semáforo para controlar el número de espacios vacíos en el buffer

void* Producer(void* arg) { // Función que representa el hilo del productor
	int *id = ((int*) arg); // Conversión del argumento void* a un puntero entero
	bool initial_registration = true; /* Variable booleana que indica si se considera fragmento de codigo
									en que se registra la generación del elemento en el archivo de registro */
	string data_element; // Variable que almacena el elemento de datos generado por el productor

	// Registra la creación del productor en el archivo de registro
	AppendLineToFile("registro_productores.txt", "productor " + to_string(*id+1) + " creado"); 
	// Ciclo que representa la generación de elementos de datos por parte del productor
	for (int i = 1; i <= NPP; i++) { 
		do {
			sem_wait(&sem_empty); // Espera a que haya espacio vacío en el buffer
			pthread_mutex_lock(&mutex_buffer); // Bloquea el acceso al buffer utilizando el mutex

			/* Sentencia condicional que evita repetir el registro de la generación de elemento 
			en caso de que la inserción del elemento no fue exitosa en periodo anterior.
			Si la inserción del elemento no fue exitosa en periodo anterior, no es necesario
			registar nuevamente el elemento generado por el hilo productor, pues se esta trabajando
			con el mismo elemento en el ciclo actual para su eventual inserción en buffer. */
			if (initial_registration){
				data_element = to_string(*id+1)+"_"+to_string(i); // Genera el elemento de datos
				// Registra la generación del elemento en el archivo de registro
				AppendLineToFile("registro_productores.txt", 
				"Hilo productor " + to_string(*id+1) + " generó " + data_element); 
			}
			p.addItem(data_element); // Intenta insertar el elemento al buffer
			// si la inserción fue exitosa
			if (p.successful_insertion){
				AppendLineToFile("registro_productores.txt", // Registra en archivo la inserción exitosa 
				to_string(*id+1) + " " + data_element + " 'Inserción exitosa'"); 
				initial_registration = true;
				sem_post(&sem_full); // Aumenta el contador de espacios ocupados en el buffer
				pthread_mutex_unlock(&mutex_buffer); // Libera el acceso al buffer
				sleep(1+rand()%5); // Espera un tiempo aleatorio
			// si la inserción no fue exitosa
			}else{
				AppendLineToFile("registro_productores.txt", // Registra en archivo el error de inserción 
				to_string(*id+1) + " " + data_element + " 'Buffer lleno - Error de inserción'"); 
				initial_registration = false;
				sem_post(&sem_empty); // Aumenta el contador de espacios vacíos en el buffer
				pthread_mutex_unlock(&mutex_buffer); // Libera el acceso al buffer
				sleep(1+rand()%4); // Espera un tiempo aleatorio
			}
		}while (p.successful_insertion == false); // Repite el ciclo si la inserción no fue exitosa
	}

	pthread_exit(NULL); // Finaliza el hilo del productor
}



void* Consumer(void* arg) { // Función que representa el hilo del consumidor
	int *id = ((int*) arg); // Conversión del argumento void* a un puntero entero
	AppendLineToFile("registro_consumidores.txt",// Registra la creación del consumidor en el archivo de registro
	 "Consumidor " + to_string(*id+1) + " creado"); 

	// Ciclo que representa la eliminación de elementos del buffer por parte del consumidor
	for (int i = 1; i <= NCC; i++) { 
   		sem_wait(&sem_full); // Espera a que haya elementos en el buffer para consumir
   	    pthread_mutex_lock(&mutex_buffer); // Bloquea el acceso al buffer utilizando el mutex
		AppendLineToFile("registro_consumidores.txt", // Registra el intento de eliminación de elemento
		"Hilo consumidor " + to_string(*id+1) + " intentando eliminar elemento de búfer"); 
		string valor = p.removeItem(); // Remueve el último elemento del buffer
		AppendLineToFile("registro_consumidores.txt", to_string(*id+1) + " " + valor +
		" eliminado con éxito."); // Registra la eliminación exitosa en el archivo de registro
		pthread_mutex_unlock(&mutex_buffer); // Libera el acceso al buffer
		sem_post(&sem_empty); // Aumenta el contador de espacios vacíos en el buffer
		sleep(1+rand()%5); // Espera un tiempo aleatorio
	}

	pthread_exit(NULL); // Finaliza el hilo del consumidor
}


int main(int argc, char *argv[]) {

	// Verifica si se proporcionaron 6 parámetros en la línea de comandos
	if (argc != 6) { 
		cout << "Debe Indicar 6 parámetros!\n";
		// Sale del programa con un código de error
		exit(1);
	}else {
		cout << "Ejecución Válida!\n";
		string cantproductor{argv[1]};	 
		string cantConsumidor{argv[2]}; 
		string tamannoBufer{argv[3]}; 
		string countproductor{argv[4]};
		string countConsumidor{argv[5]};	 

		// Conversión de tipos (int <- string)
		NP = atoi(cantproductor.c_str());
		NC = atoi(cantConsumidor.c_str());
		BC = atoi(tamannoBufer.c_str());
		NPP = atoi(countproductor.c_str());
		NCC = atoi(countConsumidor.c_str());

		// Imprime los valores de los parámetros convertidos
		cout << "NP: " << NP << " - NC: " << NC << " - BC: " << BC << \
		" - NPP: " << NCC << " - NCC: " << NPP << "\n";
		// No hay suficiente produccion de elementos para los consumidores
		if ((NP*NPP)<(NC*NCC)){
			cout <<"Ingreso invalido. Totalidad de elementos producidos es menor a";
			cout<<" cantidad de elementos consumidos."<<endl;
			exit(1);
		}
		// Asigna la capacidad maxima del búfer
		p.max = BC;
	}

	/* Creacion de archivos registro de productores y registro de consumidores 
	(trunca el archivo de ya existir) */
	ofstream file1("registro_productores.txt", std::ios::trunc);
	ofstream file2("registro_consumidores.txt", std::ios::trunc); 

	// Declaración de arreglos de hebras de productores y consumidores
	pthread_t producer[NP]; 
	pthread_t consumer[NC]; 
	// Inicialización de un mutex para controlar el acceso al buffer compartido
	pthread_mutex_init(&mutex_buffer, NULL); 
	// Inicialización de semáforos que indican la cantidad de elementos en el buffer
	// sem_full representa los espacios ocupados
	// sem_empty representa los espacios vacíos
	sem_init(&sem_full, 0, 0);
	sem_init(&sem_empty, 0, BC);
	int i;

	for (i = 0; i < NP; i++) { // Creación de hebras de productores
		int* a = new int; 
        *a = i; 
		pthread_create(&producer[i], NULL, &Producer, a); 
	}

	for (i = 0; i < NC; i++) { // Creación de hebras de consumidores
		int* a = new int; 
        *a = i;
		pthread_create(&consumer[i], NULL, &Consumer, a); 
	}

	// Espera a que todas las hebras de productores terminen, registrando en archivo
	for (i = 0; i < NP; i++) { 
		pthread_join(producer[i], NULL); // Bloquea el hilo principal hasta que el hilo del productor termine
		AppendLineToFile("registro_productores.txt","Hilo productor "+\
		to_string(i+1)+" ha terminado."); 
	}

	// Espera a que todas las hebras de consumidoras terminen, registrando en archivo
	for (i = 0; i < NC; i++) { 
		pthread_join(consumer[i], NULL); // Bloquea el hilo principal hasta que el hilo del consumidor termine
		AppendLineToFile("registro_consumidores.txt","Hilo consumidor "+\
		to_string(i+1)+" ha terminado."); 
	}

	sem_destroy(&sem_empty); // Destruye el semáforo de elementos vacíos
	sem_destroy(&sem_full); // Destruye el semáforo de elementos llenos

	file1.close(); // Cierra el archivo de registro de productores
	file2.close(); // Cierra el archivo de registro de consumidores

	return 0; // Retorna 0 para indicar una finalización exitosa del programa
}
