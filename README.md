# Light Control System
<h1>MQTT controled WS2812B leds</h1>


<h2>Done:</h2>
<ul>
  <li>Any color can be set with the MQTT command: <strong>SetColor</strong>. Load consists of 7bytes, 1 byte for each color, 2 bytes for setting the starting point, and 2 bytes for setting the ending point. Should look like this: RGBSsFf (RED, GREEN, BLUE, high side of Starting position int, low side of Starting position int, High side of Ending position int, low side of Ending position int) </li>
  <li>SetColor commands lets you concatenate loads, like this: RGBSsFfRGBSsFfRGBSsFf (3 colors in one load, each with its starting poing and ending point)</li>
  <li>An array of different colors can be sent with the MQTT command: <strong>rawSetcolor</strong> This command lets you send a load that consists of 2 bytes setting the starting point (As shown before in SetColor) then setting the final point, and then sending 3 bytes of color, per pixel. </li>
  <li>Diferent animations are per default supported. Yet, there are still more to come. Sending the MQTT command <strong>animacion</strong> and as load the number of animation, triggers the said animation.</li>
  <li>The MQTT command <strong>Fuente</strong>, sets the GPIO pin 16 (D0, in Wemos R1 mini), high or low. This is the only command that is trigered by the ASCII byte code for "1". The only reason for this is back-compability</li>
  <li>MQTT commands can be mixed together to form new animations, like rawSetColor with animation 255 (moves all the pixels in the right direction)</li>
  <li>OTA capabilities are included, but they are still being tested and debuged. A precompiled file can be uploaded to an existing server, and the esp8266 can upgrade itself. </li>
</ul> 
<h2>To do:</h2>
<ul>
  <li>More animations</li>
  <li>At some point, Music analisis will be added, so it can follow the rythm of the music</li>
  <li>Some Methods might be remade again, to use less global variables. Also there are some bugs with the circular buffer</li>
</ul> 
<h2> Avaliable Commands</h2>
<table>
  <tr>
    <th>Command</th>
    <th>Funtion</th>
    <th>Load</th>
  </tr>
  <tr>
    <td>SetColor</td>
    <td>Sets color</td>
    <td>RGBSsFf</td>
  </tr>
  <tr>
    <td>animation</td>
    <td>starts animation</td>
    <td>load: 1 Byte with the animation code</td>
  </tr>
  <tr>
    <td>rawSetColor</td>
    <td>sets an array of diferent colors</td>
    <td>SsFf(RGB)1(RGB)2(RGB)3...</td>
  </tr>
  <tr>
    <td>ActualizacionServidor</td>
    <td>Searches for the last compiled file</td>
    <td>First byte is the Object definition, second and third bytes are the version</td>
  </tr>
  <tr>
    <td>Actualizacion</td>
    <td>Starts Arduino OTA</td>
    <td></td>
  </tr>
</table>


