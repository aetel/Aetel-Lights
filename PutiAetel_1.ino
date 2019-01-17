#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>   //libreria para MQTT
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
const uint16_t PixelCount = 633;  //Creo que eran 633 leds. Habria que mirarlo igual s
#define Fuente D0                 //Este pin va conectado al transistor que enciende la fuente siempre. Activado enciende la fuente
const char* UserMqtt="lukas";
const char* PassMqtt="lukas";
#define IpMqtt "192.168.1.41"
const char* OTAServer="http://192.168.1.40/OTAServer/"; //Servidor desde donde se realizan las actualizaciones OTA
#define ssid "Zona Alfa"
#define ssidpass "no te voy a decir la clave"
WiFiClient    Luces;
PubSubClient client(Luces); //nuestro cliente son las Luces
bool Posicion =false;
byte Animacion=0;
int UltimaPosicion=0;
float parametro=0;
float dimension=0.05; //Dimension por defecto es 20 pixeles ola
static int pos=0;
int tamano=0;
static String NumActualizacion="Actualizacion15";
const int deviceversion=0;  //Estas cosas estarian guay si se guardasen en la "EEPROM" Asi distintos dispositivos puedes guardar distintos parametros
const char deviceClass[3]={'L',(deviceversion&0xFF),((deviceversion>>8)&0xFF)}; //El primer byte, es el tipo de dispositivo L, sera de Luces. Los dos siguientes bytes indicaran la version
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> LED(PixelCount); //Para manejar el unico RGB que tiene
NeoPixelAnimator animations(PixelCount); // NeoPixel animation management object
struct MyAnimationState{
  RgbColor StartingColor;
  RgbColor EndingColor;
  float frecuencia;   //Para la animacion 6
};
float contadores[35]; 
int contadores1[35];  //Estos dos contadores los tengo que unificar. A corre
unsigned long relojanimacion =0;
MyAnimationState animationState[PixelCount];  //Declara este tio las cosas como le da la puta gana
struct datosAnimSpectrum{
  RgbColor ArrayOla[50];  //Un array donde podamos meter una ola, que se vuelca
  int WaveSize;
  bool direccion=1;
};
datosAnimSpectrum Spectrum[35];
void setup() {
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid,ssidpass); //Esto lo puedo cambiar por mi propia version de Wifi Handler, la de la puerta. Por ahora solo quiero algo funcional
  Serial.begin(115200);
  WiFi.printDiag(Serial); //Para saber como rula esto
  pinMode(Fuente,OUTPUT); //Activamos la salida de la fuente
  digitalWrite(Fuente,1);
  while((WiFi.status() !=WL_CONNECTED)){
    Serial.println("No se conecta");  //Se tiene que quedar aqui hasta que se conecte, que esto siempre tarda un ratillo
    delay(100);
  }
  digitalWrite(Fuente,0);
  delay(1000);
  Serial.println("Exito");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  LED.Begin();
  LED.Show();
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    //QUE COJONES AQUI SE TIENE QUE REINICIARRRR
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    //int x= map(progress,0,total,0,PixelCount);
    colocar(RgbColor(0,0,255),0,(progress / (total / PixelCount)));
    LED.Show();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname("Luces Aetel");              //Para poder conectarnos por mqtt, y actualizar las luces
  ArduinoOTA.setPassword((const char *)"IEEEAETEL");
  ArduinoOTA.begin();
  client.setServer(IpMqtt, 1883);   //se abre el portal MQTT
  client.setCallback(Recepcion);
  SetRandomSeed(); 
}
void loop() {
  ServerManager();
  if(relojanimacion<millis()){  //Todos esos if me los puedo quitar poniendo las condiciones repetidas antes, y con un switch elejir la animacion
    switch(Animacion){
      case 1: //Espero que sepa que lo que yo le paso son bytes, not argumentos
        animacion();  //Esta animacion mete por led un color random
        break;
      case 2:
        theaterChase(RgbColor(0,0,0),RgbColor(200,80,0));
        relojanimacion=(millis()+10); //theather chase en el color naranja. Se podria hacer con cualquier color, si quisieramos
        break;
      case 3:
        theaterChaseRainbow(RgbColor(0,0,0));
        relojanimacion=(millis()+10); //theather chase rainbow
        break;
      case 4: //Fade in fade out
        if(parametro>=1){
          startFadeInFadeOut();
       }
        FadeInFadeOut();  //Hay que poner un LED.Show();
        relojanimacion=(millis()+40);
        break;
      case 5: //olas. Mola muchisimo.
        if(parametro>=1){
          startFadeInFadeOut();
          newtamano();
        }
        olasPusheadasStruct();
        relojanimacion=(millis()+15); //No debe ser ni muy lento, ni muy rapido
        break;
      case 6:
      Serial.println("entramos Spectrum");
        keepFasedSpectrum();
        relojanimacion=(millis()+10);
        break;
       case 7:
        keepColorWheelSpectrum(); //Esta animacion son como olas de un unico color que cambian de brillo, en 35 puntos
        relojanimacion=(millis()+15);
        break;
      case 255: //Rotativo. En realidad esta funcion deberia ser complementaria
        circularBuffer(1,true);
        relojanimacion=millis()+20;
        break;
    }
    LED.Show(); //esta funcion se supone que ya hace un IsDirty?
  }
}
void ServerManager(){ //Se ocupa de mantener la conexion con el servidor y de cumplir //Uff tengo que buscar el timeout de mqtt, porque si no avisa repite
  if(!Luces.connected()){ //Si no esta conectado, nos intentamos conectar
    Serial.print("Tratamos de conectarnos a Mqtt UserMqtt: ");
    Serial.print(UserMqtt);
    Serial.print(" PassMqtt ");
    Serial.println(PassMqtt);
    delay(100);
    if(client.connect("LucesAetel",UserMqtt,PassMqtt)){  //boolean connect (clientID, username, password, willTopic, willQoS, willRetain, willMessage) //Claves: puerta, ae7elh0st
      client.publish("Luces","a su servicio");
      client.subscribe("Actualizacion16"); //El primero y mas importante. El que permitira actualizar el sistema
      client.subscribe("actualizacion");
      client.subscribe("SetColor");
      client.subscribe("Fuente");
      client.subscribe("animacion");
      client.subscribe("rawSetcolor");
      client.subscribe("ActualizacionServidor");  //Este sera el especifico de actualizacion desde el servidor
      Serial.println("Se ha conectado al servidor MQTT");
    }
  }
  else{ //Si ya esta conectado, que se ocupe de las tareas del servidor
    client.loop();
  }
}
void Recepcion(char* tema,byte* carga, unsigned int length){    //MQTT
  //esta parte se dedica  a la recepcion del dato 
  //teoricamente la forma en la que recibe el dato es Destinatario/tema/carga
  //Temas a tratar: Hash, Programar Miembro, Borrar Miembro....no?
  //los temas a tratar vienen de parte del servidor. en direccion contraria, serian otros temas
  Serial.print("Tema que recibo: ");
  Serial.println(tema);
  if(strcmp(tema,"Fuente")==0){
   if((char)carga[0]=='1'){
    digitalWrite(Fuente, 1); 
   }
   else{
    digitalWrite(Fuente, 0);
   }
   delay(100);
//   RgbColor rosa=RgbColor(199,67,117);
//   colocar(rosa,0,633); 
//   LED.Show();
  }
  if (strcmp(tema, "Actualizacion16") == 0){ //renovar firmware
    //el byte definiria el dispositivo que queremos actualizar?
    //carga[0]
    OverTheAir();
    
  }
  if (strcmp(tema, "SetColor") == 0){ //Bueno, hasta ahora he hecho dos funciones, una de actualizar y uno de poner un color en toda la rama de leds
    for(int x=0; x<(length/7);x++){ //esta parte se ocupa de meter varios colores de un unico mensaje y colocarlos, en un unico movimiento
      RgbColor color =RgbColor(*(carga+x*8),*(carga+1+x*8),*(carga+2+x*8));
      int inicio = (*(carga+4+x*8) <<8)+*(carga+3+x*8);
      int ledfinal = (*(carga+6+x*8) <<8)+*(carga+5+x*8);
    //Recordatorio:Creo que en tu codigo de java, manejabas varios colores
      colocar(color, inicio, ledfinal);
    }
    //RgbColor color =RgbColor(*carga,*(carga+1),*(carga+2));
    //int inicio = (*(carga+4) <<8)+*(carga+3);
    //int ledfinal = (*(carga+6) <<8)+*(carga+5);
    //Recordatorio:Creo que en tu codigo de java, manejabas varios colores
    //colocar(color, inicio, ledfinal);
    //digitalWrite(Fuente, 1);  //Encendemos la fuente
    //delay(10);  //Ponemos un minidelay para darle tiempo a la fuente (No se si es estrictamente necesario. Habria que probar
    LED.Show();
  }
  if(strcmp(tema, "rawSetcolor")== 0){  //rawsetcolor, es una nueva funcion, que recibira 
    int inicio = (*(carga+1) <<8)+*(carga);  //inicio estara en la posicion 0  //(*(carga+4+x*8) <<8)+*(carga+3+x*8);
    int ledfinal = (*(carga+3)<<8)+*(carga+2);  //led final pos 1
    RgbColor ArrayColores[length-4];//= new RgbColor[length-2];
    for(int x=0; x<length-4;x=x+3){
      ArrayColores[(x/3)]=RgbColor(*(carga+x+4),*(carga+x+5),*(carga+x+6));
    }//metemos los datos en un array de colores
    //posicionamiento(inicio,ledfinal,ArrayColores);
    inyector(inicio, ledfinal, ArrayColores);
  }
  if (strcmp(tema, "animacion")== 0){
    Animacion=*carga;
    switch(Animacion){
      case 6:
        newFasedSpectrum();
        break;
      case 7:
        newColorWheelSpectrum();  //Este se podria hacer con mas colores por rama. La verdad es que molaria
        break;
    }
  }
  if (strcmp(tema, "ActualizacionServidor")==0){
    int versionMQTTact=(*(carga+1)+(*(carga+2)<<8));
    Serial.printf("La version que nos llega es: %c y el numero es %d", *carga, versionMQTTact);
    if(*carga==deviceClass[0]&&(deviceversion<versionMQTTact)){
      Actualizacion();
      
    }
  }
}
void OverTheAir(){  //Funcion que activa OTA. Tiempo activo: 2Min. La animacion es, va cambiando de azul, a rojo a lo largo que pasa el tiempo.
  int tSet=1000;
  unsigned long t= millis();
  unsigned long t_old=millis()+tSet;
  t=t+120000;
  Serial.println("Over the air esta activo");
  RgbColor ColorOTA1=RgbColor(0,255,0);
  RgbColor ColorOTA2=RgbColor(255,0,0);
  RgbColor color;
  colocar(ColorOTA1,0, PixelCount);
  float progreso;
  bool EstadoFuente = digitalRead(Fuente);
  digitalWrite(Fuente, 1);  //Tenemos que activar la Fuente para mostrar algo
  delay(100); //Un delay para que se encienda la fuente
  LED.Show();
  while(t> millis()){ //Mientras que no hayn pasado 2 minutos, seguira, intentando actualizarse. Esta secuencia tiene que ser non-blocking
    ArduinoOTA.handle();  //Que animacion hago para actualizar?
    delay(1);  //non-blocking
    ServerManager();  //Tiene que estar aqui, que si no se autonockea
//    if(t_old<millis()){
//      progreso=progreso+(tSet/120000);  //Pensaba hacerlo con los t, pero, si millis es alto, la escala se reduce drasticamente
//      r=255/progreso;
//      //color=RgbColor::LinearBlend(ColorOTA1, ColorOTA2, progreso);
//      color=RgbColor(r,(255-r),0);
//      colocar(color,0,PixelCount);  //Colocamos el color en la tira
//      t_old=t_old+tSet;
//      Serial.print("recolocando color: ");
//      Serial.println(progreso);
//    }
    if(LED.CanShow()&&LED.IsDirty()){ //Si ha pasado suficiente tiempo, y se ha llamado SetColor
      LED.Show(); //pues que muestre. Esto teoricamente no deberia bloquear
    }
  }
  digitalWrite(Fuente, EstadoFuente); //Volvemos al estado preliminar de la Fuente. Esto solo pasa si no llega a actualizarse
 
}
void SetFuente(bool Estado){  //Solo para encender la Fuente. Exagerado...
  digitalWrite(Fuente, Estado);
}
void colocar(RgbColor color, int a, int b){
  //Nuestra funcion mostrar, mostrara los colores del LED, desde la direccion que se ha metido, hasta la direccion que se le meta en segundo valor
  if(a>b){
    for(b;b<a;b++){
      LED.SetPixelColor(b, color);
    }
  }
  if(b>a){
    for(a;a<b;a++){
      LED.SetPixelColor(a, color);
    }
  }
}
void loadingUp(){
  //empezamos desde un color frio, al maximo y lo movemos a un color calido pasando por un color tranquilizador
  //A ver que efecto queda
  bool AnimFIN=false;
  int g=0;
  int delaytime=2;
  RgbColor color(0,0,255);//Empezamos en azul
//while(!AnimFIN){
  delay(delaytime);
  for(int i=255; i>0;i--){
    RgbColor color(0,(255-i),i);
    colocar(color, 0, 633);
    delay(delaytime);
  }
  for(int i=255; i>50;i--){
    RgbColor color1((255-i),i,0);
    colocar(color1, 0, 633);
    delay(delaytime);
  }
  for(int i=50; i>0;i--){
    RgbColor color(0,0,0);
    colocar(color, 0, 633);
    delay(100);
    RgbColor color1((255-i),i,0);
    colocar(color1, 0, 633);
    delay(200);
  }

  for(int i=255; i>0;i--){
  
    RgbColor color(i,0,(255-i));
    colocar(color, 0, 633);
    delay(delaytime);
  }
//}
}
void Tricolor(RgbColor color1, RgbColor color2,RgbColor color3){
  //Idea de perla y victor: Son tres colores que se reparten en 6 leds
  //Son 3 colores por 6 leds que son 18 leds
  int Reparto=633/18;  //Dividimos el conjunto de leds
  for(int i=1;i<Reparto;i++){ //Vamos dividiendo las seciones por colores //No me gusta
  colocar(color1,i,(6*i));
  colocar(color2,(i*7),(12*i));
  colocar(color3,(i*13),(18*i));
  }
  LED.Show();
}  
void theaterChase(RgbColor color1, RgbColor color2){//El original tiene una funcion wait. Pero nuestro codigo debe ser non-blocking. asi que: ni. de. coñaaa
  //theaterchase. Son 3 leds que son perseguidas por otra de otro color. La funcion va pasando indefinidamente led por led colocando el color, moviendo el puntero uno por pasada
  for(int i=0;i<PixelCount;i++){  //Esto se pasa led por led
    if ((i+UltimaPosicion)%5==0){
      LED.SetPixelColor(i,color1);
    }
    else{
      LED.SetPixelColor(i,color2);
    }
  }
  UltimaPosicion++;
  if(UltimaPosicion==6){
    UltimaPosicion=0;
  }
  LED.Show();
}
void theaterChaseRainbow(RgbColor color2){//El original tiene una funcion wait. Pero nuestro codigo debe ser non-blocking. asi que: ni. de. coñaaa
  //theaterchase. Son 3 leds que son perseguidas por otra de otro color. La funcion va pasando indefinidamente led por led colocando el color, moviendo el puntero uno por pasada
  for(int i=0;i<PixelCount;i++){  //Esto se pasa led por led
    if ((i+UltimaPosicion)%5==0){
      LED.SetPixelColor(i,color2);
    }
    else{
      LED.SetPixelColor(i,Rainbow((i+UltimaPosicion)%5));
    }
  }
  UltimaPosicion++;
  if(UltimaPosicion==6){
    UltimaPosicion=0;
  }
  LED.Show();
}
void RepresentacionMusical(){
  //Hmmm, bueno, esto va a ser dificil. Tenemos dos formas de hacerlo... bueno 3, pero dos, que son... realizables bajo estas condiciones
  //Una opcion seria, mirar el analog 0, y por el ruido, que recibe el microfono, poner un color u otro.
  //La otra opcion que a mi me gusta, y que se podria hacer de varias maneras, seria hacer una FFT de la musica. Los majetes de spotify, tienen una funcion en su API, que hace
  //Analisis de la musica Mas informacion: https://developer.spotify.com/console/get-audio-analysis-track/?id=06AKEBrKUckW0KREUWRnvT (uff uff... mola demasiado...)
}
void FadeInFadeOut(){
  //Funcion propia con lo que entiendo de las funciones de makuna
  RgbColor color=RgbColor::LinearBlend(animationState[0].StartingColor,animationState[0].EndingColor,parametro);
  colocar(color,0, 633);
  parametro=parametro + 0.01; //voy a poner aqui un punto, porque creo que esta era la parte que fallaba
}
float startFadeInFadeOut(){ //Solo
  animationState[0].StartingColor=LED.GetPixelColor(0);
  animationState[0].EndingColor=RgbColor(random(200),random(200),random(200));
  parametro=0;
}
void newtamano(){
  //tamano=random(10,50); //Idealmente, el tamano deberia estar fijado por la diferencia de colores inicial y final. Si hay poca diferencia, deberia ser pequeño
  tamano=40*triangulo(animationState[0].StartingColor,animationState[0].EndingColor)/200; //Esto dara un tamaño proporcional a la diferencia de dos colores
  Serial.printf("tamaño: %d",tamano);
  Serial.println();
  dimension=1.0/(float)tamano;
}
void posicionamiento(int inicio, int ledfinal,RgbColor *ArrayColores){ //Posiciona el array de colores
  //Posicionamos el array de colores en la tira
  if(inicio<ledfinal){
    for(int x=0;x<(ledfinal-inicio);x++){
      LED.SetPixelColor(inicio+x,*(ArrayColores+x));
    }
  }
  else{
    for(int x=0;x<(inicio-ledfinal);x++){
      LED.SetPixelColor(ledfinal+x,*(ArrayColores+x));
    }
  }
  LED.Show();
}
void circularBuffer(int posiciones, bool direccionDerecha){
  //Se hace asi? noup Lo hare asi? sep
  if(direccionDerecha){
    LED.RotateRight(posiciones);  //Esto se supone que mueve los piexeles en determinada direccion
    pos=pos+posiciones;
    if(pos>PixelCount){//Esto mantiene la posicion del pixel 0. La idea es que las luces se mueven pero la posicion inicial se mantiene.
      pos=pos-PixelCount;    
    }
  }
  else{
    LED.RotateLeft(posiciones);
    pos=pos-posiciones;
    if(pos<0){
      pos=PixelCount-pos; //I believe this is right
    }
  }
 LED.Show();  //mostramos. Deberia hacerse fuera de la funcion?
}
void inyector(int inicio,int ledfinal, RgbColor *ArrayColores){
  //insertamos los colores en el buffer de colores //usamos variable pos
  int iniciof=inicio+pos;
  int ledfinalf=ledfinal+pos;
  if(iniciof>PixelCount){
    iniciof=iniciof-PixelCount;
  }
  if(ledfinalf>PixelCount){
    ledfinalf=ledfinalf-PixelCount;
  }
  posicionamiento(iniciof,ledfinalf,ArrayColores);
}
void olasRandom(){
  //Creamos Arrays de dimension aleatoria, con diferentes colores
  
}
void olasPusheadas(){ //Este metodo es un metodo blocking. Mejor diseñar otro metodo
  int dimension=random(10,50);
  //RgbColor color1=RgbColor(random(255),random(255),random(255));  //El color que debe contener color1 es el color del ultimo pixel escrito.
  RgbColor color1=LED.GetPixelColor(0);
  RgbColor color2=RgbColor(random(150),random(150),random(150));
  for(int h=0; h<dimension; h++){
    //empujamos los colores a una velocidad determinada
    LED.RotateRight(1);
    LED.SetPixelColor(0,RgbColor::LinearBlend(color1,color2,((float)h/(float)dimension)));
    LED.Show();
    
  }
}
void olasPusheadasStruct(){
  LED.RotateRight(1);
  LED.SetPixelColor(0,RgbColor::LinearBlend(animationState[0].StartingColor,animationState[0].EndingColor,parametro));
  parametro=parametro+dimension;
  }
void Actualizacion(){
  Serial.println("La version es 0");
  yield();  //de verdad que en medio de esto no queremos que nada nos interrumpa
  String Servidor=String (OTAServer);
  Servidor.concat(String(deviceClass[0]));
  HTTPClient cliente;
  cliente.begin(Servidor);
  int respuesta=cliente.GET();
  //Serial.printf("La respuesta del cliente a sido %d", respuesta);
  switch(respuesta){
    case 200:{ //Existe
      String versiondisponible = cliente.getString();
      int numversion=versiondisponible[1]+(versiondisponible[2]<<8);  //Esto nos deberia dar la version
      //Serial.printf("El nombre de la version disponible es: %s y la version es: %d", versiondisponible.c_str(), numversion);
      if(numversion>deviceversion){ //Si esto se cumple, podemos actualizar
        digitalWrite(Fuente,true);
        delay(100);
        colocar(RgbColor(0,100,0), 0, PixelCount);  //Vamos a indicar que se actualiza con un ligero verde
        LED.Show();
        delay(1000);  //Esperamos un segundin, 
        Servidor=String(OTAServer);
        Servidor.concat(versiondisponible);
        //Servidor.concat(".bin");
        //versiondisponible=versiondisponible.concat(".bin");
        Serial.print(Servidor);
        t_httpUpdate_return ret = ESPhttpUpdate.update( "http://192.168.1.40/OTAServer/L1.bin" );  //Teoricamente esta cosa actualiza
        Serial.printf("FALLO EN: %d", ret);
        switch(ret){
          case HTTP_UPDATE_FAILED:  //Bueno, ya se me ocurrira que hacer con estos casos
            Serial.println("Fallo la actualizacion");
            Warning();
            break;
          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No hay actualizaciones disponibles");
            break;
          case HTTP_UPDATE_OK:
            Serial.println("Se actualizo"); 
        }
      }
      break;
    }
    case 404:{
      Serial.println("No page was found");
      break;
    }
    case 301:{
      Serial.println("Redireccion? nO se la verdad");
      break;
    }
  }
  Warning();
  cliente.end();
}
void Warning(){
  for(int h=0;h<5;h++){
    colocar(RgbColor(100,0,0),0,PixelCount);
    LED.Show();
    delay(300);
    colocar(RgbColor(0,0,0),0,PixelCount);
    LED.Show();
    delay(300);
  }
}
int triangulo(RgbColor color1, RgbColor color2){
  /*Supongamos que los colores se pueden representar en un triangulo, donde existen 3 tensores. El color central seria el blanco, El negro seria otro color central.. vaya.
  Bueno esto es como calcular la diferencia entre coordenadas delta. It's never to late to learn something new No se como hacerlo *facepalm* 
  //Vale, idea 1: Calculamos el indice de tension central. Esto es, la diferencia que hay de un color hacia el blanco, y luego calculamos el gradiente.
  //Idea 2. Posicionamos en un plano cartesiano 2D los 2 colores. Esto lo seguimos haciendo con los tensores triangulares, y calculamos la distancia
  //Idea 3: PLano cartesiano 3D. El plano cartesiano 3D separa completamente los 3 colores, convirtiendolos en variables unicas, pero centrados en el (0,0,0)*/
  int dif[3];//R,G,B
  if(color1.R>color2.R){
    dif[0]=color1.R-color2.R;
  }
  else{
    dif[0]=color2.R-color1.R;
  }
  if(color1.G>color2.G){
    dif[1]=color1.G-color2.G;
  }
  else{
    dif[1]=color2.G-color1.G;
  }
  if(color1.B>color2.B){
    dif[2]=color1.B-color2.B;
  }
  else{
    dif[2]=color2.B-color1.B;
  }
  int r1=sqrt((dif[0]*dif[0])+(dif[1]*dif[1]));
  int r2=sqrt((r1*r1)+(dif[2]*dif[2]));
  return r2;  //devolvemos la distancia entre 2 puntos en un plano cartesiano 3D, que es casi equivalente a un plano delta. Si aplanamos el volumen en un plano 2D
}
void newFasedSpectrum(){
  int conjuntodeespectros=random(20,35); //No podemos ni ampliarlo demasiado, o comprimirlo demasiado
  animationState[0].frecuencia=float(random(20,100))/500.0;
  tamano=conjuntodeespectros;  //Tenemos que saber en el programa original cuantas montañas tenemos. Las llamo montañas, porque son puntos de interseccion, y afluentes
  for(int h=0; h<conjuntodeespectros; h++){
    animationState[h+1].frecuencia=float(random(20,100))/500.0;
    contadores[h]=0.5;  //Vamos a ponerle un valor inicial, para que no se quede atascado por debajo del 0.2, en la siguiente funcion
    Serial.print("FRECUENCIA: ");
    Serial.println(animationState[h].frecuencia);
    do{
      animationState[h+1].StartingColor=RgbColor(random(200),random(200),random(200));
    }while(120>triangulo(animationState[h].StartingColor,animationState[h+1].StartingColor)); //Mientras que la diferencia de colores no supere 120, se mantiene aqui, asi se mezclan un poco los colores
  }
}
void keepFasedSpectrum(){
  int posicionamiento=PixelCount/tamano;  //cada x posicionamiento colocamos el siguiente Espectro
  Serial.printf("Posicionamiento tiene tamaño: %d", posicionamiento);
  for(int h=0;h<tamano;h++){
    contadores[h]=contadores[h]+animationState[h].frecuencia; //Añadimos sobre una base la componente de variacion
    if(contadores[h]>1||contadores[h]<0.2){
      animationState[h].frecuencia=-animationState[h].frecuencia; //Le damos la vuelta y ya esta
    }
    Serial.print("Contadores: ");
    Serial.println(contadores[h]);
    RgbColor Espectro=RgbColor(contadores[h]*animationState[h].StartingColor.R,contadores[h]*animationState[h].StartingColor.G,contadores[h]*animationState[h].StartingColor.B);
    LED.SetPixelColor(h*posicionamiento,Espectro);
    //Serial.printf("Contador h %d 
    LED.Show(); //Este paso no se si es completamente necesario, y si puedo lo quitare, pero todavia no se muy bien como funciona GetColor... Si no lo quito siguen siendo 10 ms
    int posicionprevia=(posicionamiento-1)*h;
    if(h==0){ 
      for(int j=PixelCount-posicionamiento;j>0;j++){  //Recuerda 0, no es mas grande que 633
        LED.SetPixelColor(j,RgbColor::LinearBlend(LED.GetPixelColor(PixelCount-posicionamiento),0,float(1.0-float(PixelCount-j/PixelCount))));
        if(j==PixelCount){
          break;
        }
      }
    }
    else{
      for(int j=posicionamiento*(h-1);j<posicionamiento*h;j++){
        LED.SetPixelColor(j,RgbColor::LinearBlend(LED.GetPixelColor(posicionamiento*(h-1)),LED.GetPixelColor(posicionamiento*h),(float(j)/float(posicionamiento*h))));
      }
    }
    
  }
}
void newColorWheelSpectrum(){
  /*en X arrays almacenamos todos los datos del color. En este modelo no modificamos la luminosidad. El proceso anterior no estaba mal, pero es complejo, y largo.
   * ESte proceso.... Sera... no lo se. POrque aunque el spectro sea un array de un color (o varios), que se mueven a una velocidad constante, pero con variacion diferente,
   * de un spectro al siguiente, no se si hacer un linear blend, o hacer un shift del anterior color. Aun asi, ambas animaciones que acabo de describir serian bastante molonas
   * So lets do it
   */
   Spectrum[0].ArrayOla[0]=RgbColor(random(150),random(150),random(150)); //Metemos el primer color
   for(int h=1; h<35;h++){
    do{
      Spectrum[h].ArrayOla[0]=RgbColor(random(150),random(150),random(150));
      yield();//Metemos los colores consecuyentes. Todos los colores son random, y de uno al prox, hay min una dif 120
    }while(120>triangulo(Spectrum[h-1].ArrayOla[0],Spectrum[h].ArrayOla[0]));
   }  //Una vez salimos tenemos un array en [0] con colores distintos. Ahora tenemos repartir la luminosidad a lo largo del tiempo
   for(int h=0; h<35;h++){
    Spectrum[h].WaveSize=random(20,50); //Estas son las escalas de luminosidad, Si es mas pequeño, mas elevada sera la diferencia de luminosidad. LO mismo al contrario
    for(int j=1; j<Spectrum[h].WaveSize; j++){
      Spectrum[h].ArrayOla[j]=RgbColor::LinearBlend(Spectrum[h].ArrayOla[0],RgbColor(0,0,0),1.0-float(j/(Spectrum[h].WaveSize)*1.2));
    } //La funcion de encima, rellena hasta el tamaño designado, el color[0] fadeado hacia el negro
    yield();  //Vamos a dejar que se ocupe un poco de si mismo
   }
   //Pues ale, esta funcion rellena 35 arrays de 35 colores fadeados hacia el negro. Haz lo que quieras con esto
}
void keepColorWheelSpectrum(){
  //Para que os imagineis que estoy intentando aqui. ES como mover 8 servos distintos a distintas velocidades con un 74HC595. La unica diferencia es que esto tiene mas Bits
  //Cool, isn't it?
  for(int h=0;h<35;h++){
    if(Spectrum[h].direccion){
      contadores[h]=contadores[h]+1;
    }
    else{
      contadores[h]=contadores[h]-1;
    }
    if(contadores1[h]>Spectrum[h].WaveSize||contadores1[h]==0){ //Aqui puede haber un error. Si parpadea a negro, esta aqui el error
      !Spectrum[h].direccion; //Negamos la direccion
    }
    LED.SetPixelColor(h*18,Spectrum[h].ArrayOla[contadores1[h]]);  
  }//Tras salir de aqui, tenemos los pixeles escritos. Ahora tenemos que escribir los pixeles que estan en medio No se si necesito: LED.Show();
  for(int h=0;h<34;h++){
    for(int j=0;j<18;j++){
      LED.SetPixelColor(((h+1)*j)+1,RgbColor::LinearBlend(LED.GetPixelColor(h*j),LED.GetPixelColor(18+(h*j)),float(j)/18.0));
    }
  }
}
void SetRandomSeed(){
    uint32_t seed;
    seed = analogRead(0);
    delay(1);
    for (int shifts = 3; shifts < 31; shifts += 3){
        seed ^= analogRead(0) << shifts;
        delay(1);
    }
    randomSeed(seed);
}
void BlendAnimUpdate(const AnimationParam& param){
    RgbColor updatedColor = RgbColor::LinearBlend(animationState[param.index].StartingColor,animationState[param.index].EndingColor,param.progress);
    LED.SetPixelColor(param.index, updatedColor);
}
void PickRandom(float luminance){  //Con opcion elijo
  //if(opcion){ //Cuando esto es true, pone valores random a cada led de cada tira
    uint16_t count = random(PixelCount);
    while (count > 0){
        uint16_t pixel = random(PixelCount);
        uint16_t time = random(100, 400);
        animationState[pixel].StartingColor = LED.GetPixelColor(pixel);
        animationState[pixel].EndingColor = HslColor(random(360) / 360.0f, 1.0f, luminance);
        animations.StartAnimation(pixel, time, BlendAnimUpdate);
        count--;
    }
  
}
RgbColor Rainbow(int i){
  switch (i){
    case 0:
    return RgbColor(255,0,0);
    case 1:
    return RgbColor(200,80,0);
    case 2:
    return RgbColor(180,180,0);
    case 3:
    return RgbColor(0,255,0);
    case 4:
    return RgbColor(0,0,255);
    case 5:
    return RgbColor(130,0,130);
  }
}
void animacion(){
  
  if (animations.IsAnimating()){
        animations.UpdateAnimations();
        LED.Show();
    }
    else
    {
        // no animations runnning, start some 
        //
        PickRandom(0.2f); // 0.0 = black, 0.25 is normal, 0.5 is bright
    }
}
/*
 * if (Animacion ==B00000001){
    //Serial.println("pero hemos entrado aqui?");
    //Tendria que hacer un swich//Pero para mas tarde
    animacion();
  }
  if (Animacion ==B00000010){
    if(relojanimacion<millis()){
      theaterChase(RgbColor(0,0,0),RgbColor(200,80,0));
      relojanimacion=(millis()+10);
    }
  }
  if (Animacion ==B00000011){
    if(relojanimacion<millis()){
      theaterChaseRainbow(RgbColor(0,0,0));
      relojanimacion=(millis()+10);
    }
  }
  if (Animacion ==B00000100){
    if(relojanimacion<millis()){
      if(parametro>=1){
        startFadeInFadeOut();
      }
      FadeInFadeOut();
      LED.Show();
      relojanimacion=(millis()+40);
    }
  }
  if (Animacion ==B00000101){
    if(relojanimacion<millis()){
      circularBuffer(1,true);
      relojanimacion=millis()+20;
    }
  }
  if (Animacion==B00000110){
    if(relojanimacion<millis()){
      if(parametro>=1){
        startFadeInFadeOut();
      }
      olasPusheadasStruct();
      LED.Show();
      relojanimacion=(millis()+15); //No debe ser ni muy lento, ni muy rapido
    }
  }*/
