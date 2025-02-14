
#include <ESP8266WiFi.h>

//Cantidad maxima de clientes es 1
#define MAX_SRV_CLIENTS 1
//Puerto por el que escucha el servidor
#define PORT 7070

/*
 * ssid: Nombre de la Red a la que se va a conectar el Arduino
 * password: Contraseña de la red
 * 
 * Este servidor no funciona correctamente en las redes del TEC,
 * se recomienda crear un hotspot con el celular
 */
const char* ssid = "SebastianMora";//Nombre de la red a la que el Node se conectará.
const char* password = "xd123456"; //Contraseña de la red Wi-Fi


// servidor con el puerto y variable con la maxima cantidad de 

WiFiServer server(PORT);
WiFiClient serverClients[MAX_SRV_CLIENTS];

/*
 * Intervalo de tiempo que se espera para comprobar que haya un nuevo mensaje
 */
unsigned long previousMillis = 0, temp = 0;
const long interval = 100;

/*
 * Pin donde está conectado el sensor de luz
 * Señal digital, lee 1 si hay luz y 0 si no hay.
 */
#define ldr D7 //
/*
 * Pin para leer el nivel de bateria 
 */
#define Batt A0 //
/**
 * Variables para manejar las luces con el registro de corrimiento.
 * Utilizan una función propia de Arduino llamada shiftOut.
 * shiftOut(ab,clk,LSBFIRST,data), la función recibe 2 pines, el orden de los bits 
 * y un dato de 8 bits.
 * El registro de corrimiento tiene 8 salidas, desde QA a QH. Nosotros usamos 6 de las 8 salidas
 * Ejemplos al enviar data: 
 * data = B00000000 -> todas encendidas
 * data = B11111111 -> todas apagadas
 * data = B00001111 -> depende de LSBFIRST o MSBFIRST la mitad encendida y la otra mitad apagada
 */
#define ab  D6 //
#define clk D8 //
/*
 * Variables para controlar los motores.
 * EnA y EnB son los que habilitan las salidas del driver.
 * EnA = 0 o EnB = 0 -> free run (No importa que haya en las entradas el motor no recibe potencia)
 * EnA = 0 -> Controla la potencia (Para regular la velocidad utilizar analogWrite(EnA,valor), 
 * con valor [0-1023])
 * EnB = 0 -> Controla la dirección, poner en 0 para avanzar directo.
 * In1 e In2 son inputs de driver, controlan el giro del motor de potencia
 * In1 = 0 ∧ In2 = 1 -> Moverse hacia adelante
 * In1 = 1 ∧ In2 = 0 -> Moverse en reversa
 * In3 e In4 son inputs de driver, controlan la dirección del carro
 * In3 = 0 ∧ In4 = 1 -> Gira hacia la izquierda
 * In3 = 1 ∧ In4 = 0 -> Gira hacia la derecha
 */
#define EnA D4 // 
#define In1 D3 // 
#define In2 D2 // 
#define In3 D1 // 
#define EnB D5 // 
#define In4 D0 //

byte data = B11111111;

/**
 * Variables
 */
// #AGREGAR VARIABLES NECESARIAS 
/**
 * Función de configuración.
 * Se ejecuta la primera vez que el módulo se enciende.
 * Si no puede conectarse a la red especificada entra en un ciclo infinito 
 * hasta ser reestablecido y volver a llamar a la función de setup.
 * La velocidad de comunicación serial es de 115200 baudios, tenga presente
 * el valor para el monitor serial.
 */
void setup() {
  Serial.begin(115200);
  pinMode(In1,OUTPUT);
  pinMode(In2,OUTPUT);
  pinMode(In3,OUTPUT);
  pinMode(In4,OUTPUT);
  pinMode(EnA,OUTPUT);
  pinMode(EnB,OUTPUT);
  pinMode(clk,OUTPUT);
  pinMode(ab,OUTPUT);
  pinMode(ldr,INPUT);
  pinMode(Batt,INPUT);

  // ip estática para el servidor
  IPAddress ip(172,20,10,7);//Se introduce la ip de la red a la cual se va a conectar el NodeMCU
  IPAddress gateway(172,20,10,1);
  IPAddress subnet(255,255,255,240);

  WiFi.config(ip, gateway, subnet);

  // Modo para conectarse a la red
  WiFi.mode(WIFI_STA);
  // Intenta conectar a la red
  WiFi.begin(ssid, password);
  
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) delay(500);
  if (i == 21) {
    Serial.print("\nCould not connect to: "); Serial.println(ssid);
    while (1) delay(500);
  } else {
    Serial.print("\nConnection Succeeded to: "); Serial.println(ssid);
    Serial.println(".....\nWaiting for a client at");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Port: ");
    Serial.print(PORT);
  }
  server.begin();
  server.setNoDelay(true);

  shiftOut(ab, clk, LSBFIRST, data);


}

/*
 * Función principal que llama a las otras funciones y recibe los mensajes del cliente
 * Esta función comprueba que haya un nuevo mensaje y llama a la función de procesar
 * para interpretar el mensaje recibido.
 */
void loop() {
  
  unsigned long currentMillis = millis();
  uint8_t i;
  //check if there are any new clients
  if (server.hasClient()) {
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i].connected()) {
        if (serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        continue;
      }
    }
    //no free/disconnected spot so reject
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      // El cliente existe y está conectado
      if (serverClients[i] && serverClients[i].connected()) {
        // El cliente tiene un nuevo mensaje
        if(serverClients[i].available()){
          // Leemos el cliente hasta el caracter '\r'
          String mensaje = serverClients[i].readStringUntil('\r');
          // Eliminamos el mensaje leído.
          serverClients[i].flush();
          
          // Preparamos la respuesta para el cliente
          String respuesta; 
          procesar(mensaje, &respuesta);
          Serial.println(mensaje);
          // Escribimos la respuesta al cliente.
          serverClients[i].println(respuesta);
        }  
        serverClients[i].stop();
      }
    }
  }
}

/*
 * Función para dividir los comandos en pares llave, valor
 * para ser interpretados y ejecutados por el Carro
 * Un mensaje puede tener una lista de comandos separados por ;
 * Se analiza cada comando por separado.
 * Esta función es semejante a string.split(char) de python
 * 
 */
void procesar(String input, String * output){
  //Buscamos el delimitador ;
  Serial.println("Checking input....... ");
  int comienzo = 0, delComa, del2puntos;
  bool result = false;
  delComa = input.indexOf(';',comienzo);
  
  while(delComa>0){
    String comando = input.substring(comienzo, delComa);
    Serial.print("Processing comando: ");
    Serial.println(comando);
    del2puntos = comando.indexOf(':');
    /*
    * Si el comando tiene ':', es decir tiene un valor
    * se llama a la función exe 
    */
    if(del2puntos>0){
        String llave = comando.substring(0,del2puntos);
        String valor = comando.substring(del2puntos+1);

        Serial.print("(llave, valor) = ");
        Serial.print(llave);
        Serial.println(valor);
        //Una vez separado en llave valor 
        *output = implementar(llave,valor);
    }
    else if(comando == "sense"){
      *output = getSense();         
    }

    else if (comando=="Circle"){
      digitalWrite(In3,HIGH);
      digitalWrite(In4,LOW);//Derecha
      analogWrite(EnB,1023);
      digitalWrite(In1,HIGH);
      digitalWrite(In2,LOW);
      analogWrite(EnA,1023);
      delay(10000);
      digitalWrite(In1,LOW);
      digitalWrite(In2,LOW);
      analogWrite (EnA,0);
      digitalWrite(In3,LOW);
      digitalWrite(In4,LOW);//Derecha
      analogWrite(EnB,0);
    }

    else if (comando== "ZigZag"){
      Serial.println("ZigZag");
      digitalWrite(In3, HIGH);
      digitalWrite(In4, LOW);
      analogWrite(EnB, 1023);// Derecha 
      digitalWrite(In1, HIGH);
      digitalWrite(In2, LOW);
      analogWrite(EnA,850); // Avanzar
      delay(1500);
      digitalWrite(In3, LOW);
      digitalWrite(In4, HIGH);
      analogWrite(EnB, 1023); //Izquierda
      digitalWrite(In1,HIGH);
      digitalWrite(In2,LOW);
      analogWrite(EnA,850);//Avanzar
      delay(1500);
      digitalWrite(In3,HIGH);
      digitalWrite(In4,LOW);
      analogWrite(EnB,1023);//Derecha
      digitalWrite(In1,HIGH);
      digitalWrite(In2,LOW);
      digitalWrite(EnA,850);//Avanzae
      delay(1500);
      digitalWrite(In3,LOW);
      digitalWrite(In4,HIGH);
      analogWrite(EnB,1023);//Izquierda
      digitalWrite(In1,HIGH);
      digitalWrite(In2,LOW);
      analogWrite(EnA,850);//Avanzar
      delay(1500);
      digitalWrite(In1,LOW);//Detenerse
      digitalWrite(In2,LOW);
      analogWrite(EnA,0);
      digitalWrite(In3,LOW);//Volver a la direccion 0
      digitalWrite(In4,LOW);
      analogWrite(EnA,0);
  } 

   else if (comando =="Infinite"){
    digitalWrite(In3,HIGH);
    digitalWrite(In4,LOW);
    analogWrite(EnB,1023);//Derecha
    digitalWrite(In1,HIGH);
    digitalWrite(In2,LOW);
    analogWrite(EnA,1023);//Avanzar
    delay(6000);
    digitalWrite(In3,LOW);
    digitalWrite(In4,HIGH);
    analogWrite(EnB,1023);//Izquierda
    digitalWrite(In1,HIGH);
    digitalWrite(In2,LOW);
    analogWrite(EnA,1023);//Avanzar
    delay(6000);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);//Detenerse
    analogWrite(EnA,0);
    digitalWrite(In3,LOW);
    digitalWrite(In4,LOW);
    analogWrite(EnB,0);//Direccion 0
  }

  else if (comando=="Especial"){
    digitalWrite(In1,HIGH);
    digitalWrite(In2,LOW);
    analogWrite(EnA,1023);//Avanzar
    delay(2500);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);//Detenerse
    analogWrite(EnA,0);
    delay(200);
    digitalWrite(In1,LOW);
    digitalWrite(In2,HIGH);
    analogWrite(EnA,1023);//Retroceder
    delay(1000);
    digitalWrite(In3,HIGH);
    digitalWrite(In4,LOW);
    analogWrite(EnB,1023);//Derecha
    digitalWrite(In1,LOW);
    digitalWrite(In2,HIGH);
    analogWrite(EnA,1023);//Avanzar
    delay(4000);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);//Detenerse
    analogWrite(EnA,0);
    digitalWrite(In3,LOW);
    digitalWrite(In4,LOW);
    analogWrite(EnB,0);//Direccion 
  }
  else if (comando=="Celebration1"){
    digitalWrite(In1,HIGH);
    digitalWrite(In2,LOW);
    analogWrite(EnA,950);
    delay(3000);
    digitalWrite(In3,HIGH);
    digitalWrite(In4,LOW);
    analogWrite(EnB,1023);
    delay(3000);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);
    analogWrite(EnA,0);
    delay(500);
    digitalWrite(In1,LOW);
    digitalWrite(In2,HIGH);
    analogWrite(EnA,950);
    delay(1500);
    digitalWrite(In3,LOW);
    digitalWrite(In4,HIGH);
    analogWrite(EnB,1023);
    delay(1500);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);
    analogWrite(EnA,0);
    digitalWrite(In3,LOW);
    digitalWrite(In4,LOW);
    analogWrite(EnB,0);
    
  }

  else if (comando =="Celebration2"){
    digitalWrite(In1,LOW);
    digitalWrite(In2,HIGH);
    analogWrite(EnA,950);
    delay(2000);
    digitalWrite(In3,HIGH);
    digitalWrite(In4,LOW);
    analogWrite(EnB,1023);
    delay(2500);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);
    analogWrite(EnA,0);
    delay(500);
    digitalWrite(In1,HIGH);
    digitalWrite(In2,LOW);
    analogWrite(EnA,950);
    delay(1500);
    digitalWrite(In1,LOW);
    digitalWrite(In2,LOW);
    analogWrite(EnA,0);
    digitalWrite(In3,LOW);
    digitalWrite(In4,LOW);
    analogWrite(EnB,0);
  
  }
  
    /**
     * ## AGREGAR COMPARACIONES PARA COMANDOS SIN VALOR
     * EJEM: else if (comando == CIRCLE) {
     *  
     * } 
     */
    else{
      Serial.print("Comando no reconocido. Solo presenta llave");
      *output = "Undefined key value: " + comando+";";
    }
    comienzo = delComa+1;
    delComa = input.indexOf(';',comienzo);
  }
}

String implementar(String llave, String valor){
  /**
   * La variable result puede cambiar para beneficio del desarrollador
   * Si desea obtener más información al ejecutar un comando.
   */
  String result="ok;";
  Serial.print("Comparing llave: ");
  Serial.println(llave);
  int Valor;
  Valor=valor.toInt();//Pasa valor a un int ara poder meterlo en el código.
  if(llave == "pwm"){
    Serial.print("Move....: ");
    Serial.println(valor);
    
    //# AGREGAR PARA CÓDIGO PARA MOVER EL CARRO HACIA DELANTE Y ATRAS
    if (Valor>0 && Valor<=1023){
      digitalWrite(In1, HIGH); 
      digitalWrite(In2, LOW);
      analogWrite(EnA, Valor);//La velocidad depende del número que se introduzca aquí

    }
    else if (Valor<0 && Valor>=-1023){
      digitalWrite(In1, LOW);
      digitalWrite(In2, HIGH);
      analogWrite(EnA, -Valor);

      data =  data & 0b11001111;
    }
    else if(Valor==0){
      digitalWrite(In1, LOW);
      digitalWrite(In2, LOW);
      analogWrite(EnA, 0);

      data = data | 0b00110000;
      }
    else{
      Serial.println("El valor debe de estar entre el rango 1023 y -1023");
    }
  }
 
  else if(llave == "dir"){
    switch (valor.toInt()){
      case 1:
        Serial.println("Girando derecha");
        //# AGREGAR CÓDIGO PARA GIRAR DERECHA
        {
      digitalWrite(In3, HIGH);
      digitalWrite(In4, LOW);
      analogWrite(EnB, 1023);
      }
        break;
      case -1:
        Serial.println("Girando izquierda");
        //# AGREGAR CÓDIGO PARA GIRAR IZQUIERDA
        {
      digitalWrite(In3, LOW);
      digitalWrite(In4, HIGH);
      analogWrite(EnB, 1023);
      }
        break;
       default:
        Serial.println("directo");
        //# AGREGAR CÓDIGO PARA NO GIRAR 
        {
      digitalWrite(In3, LOW);
      digitalWrite(In4, LOW);
      analogWrite(EnB, 0);
      }
        break;
    }
  }


  else if(llave[0] == 'l'){
    Serial.println("Cambiando Luces");
    Serial.print("valor luz: ");
    Serial.println(valor);
    //Recomendación utilizar operadores lógico de bit a bit (bitwise operators)
    switch (llave[1]){
      case 'f':
        Serial.println("Luces frontales");
        //# AGREGAR CÓDIGO PARA ENCENDER LUCES FRONTALES
        if (valor=="0"){
            data= data | 0b00001100; // Cuando se usa | es para cambiar un 0 por un 1
            Serial.println(data);
        }
        else if (valor=="1"){
            data= data & 0b11110011; //Cuando se usa & es para cambiar un 1 por un 0
            Serial.println(data);
          }
        break;
      case 'b':
        Serial.println("Luces traseras");
        //# AGREGAR CÓDIGO PARA ENCENDER O APAGAR LUCES TRASERAS
        if (valor=="0"){
            data= data | 0b00110000;
            Serial.println(data);
        }
        else if (valor=="1"){
            data= data & 0b11001111;
            Serial.println(data);
          }
        break;
      case 'l':
        Serial.println("Luces izquierda");
        //# AGREGAR CÓDIGO PARA ENCENDER O APAGAR DIRECCIONAL IZQUIERDA
        if (valor=="0"){
            data= data | 0b10000000;
            Serial.println(data);
        }
        else if (valor=="1"){
            data= data & 0b01111111;
            Serial.println(data);
          }
        break;
      case 'r':
        Serial.println("Luces derechas");
        //# AGREGAR PARA CÓDIGO PARA ENCENDER O APAGAR DIRECCIONAL DERECHA
         if (valor=="0"){
            data= data | 0b01000000;
            Serial.println(data);
        }
        else if (valor=="1"){
            data= data & 0b10111111;
            Serial.println(data);
          }
        break;
      /**
       * # AGREGAR CASOS CON EL FORMATO l[caracter]:valor;
       * SI SE DESEAN manejar otras salidas del registro de corrimiento
       */
      default:
        Serial.println("No corresponde a ninguna de los comandos correspondientes para las luces");
        
        break;
    }

  }
  
  /**
   * El comando tiene el formato correcto pero no tiene sentido para el servidor
   */
  else{
    result = "Undefined key value: " + llave+";";
    Serial.println(result);
  }
  shiftOut(ab, clk, LSBFIRST, data);
  return result;
}  
/**
 * Función para obtener los valores de telemetría del auto
 */
String getSense(){
  //# EDITAR CÓDIGO PARA LEER LOS VALORES DESEADOS
  int batteryLvl = digitalRead(Batt);
  int light = analogRead(ldr);

  if (light==1023){
    light=1;
  }
  // EQUIVALENTE A UTILIZAR STR.FORMAT EN PYTHON, %d -> valor decimal
  char sense [16];
  //Serial.printf(batteryLvl);
  sprintf(sense, "blvl:%d;ldr:%d;", batteryLvl, light);
  Serial.print("Sensing: ");
  Serial.println(sense);
  return sense;
}
