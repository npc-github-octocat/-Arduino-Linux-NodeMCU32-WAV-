#include <WiFi.h>
#include <string.h>
#include <driver/i2s.h>

#define N 512
#define N2 1024


const char * ssid = "your-ssid";
const char * password = "your-password";


//服务器程序运行所处的IP地址
const char * host = "xxx.xxx.xxx.xxx";
const int my_ftpPort = 8989;


int value = 0;
char commd[N] = {0};
char i2s_wr_buffer[N2] = {0};
unsigned int nbytes = 0; 
WiFiClient client;
char MusicData_Flag = 0; //接收完元数据后，开始接收音频数据，以该标志进行判断



/* 函数声明 */
void commd_help(void);
void commd_exit(void);
void commd_ls(WiFiClient client, char *);
void commd_get(WiFiClient client, char *);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);


  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print("connection to Ftp server: ");
  Serial.println(host);

  while(1){
    int i = 0;
    bzero(commd, N);
    
    // 创建TCP连接 
    if(!client.connect(host, my_ftpPort)){
      Serial.println("connection Ftp failed, please push RST key and whether open Ftp server");
      while(1);
    }
    else{
      Serial.println("\n[ Ftp connection success ]");
    }
    
    Serial.printf("ftp>");

    mySerial_Recv(commd);
    Serial.printf("Input Command Is [ %s ]\n", commd);

    if(strncmp(commd, "help", 4) == 0){
      commd_help();
    }
    else if(strncmp(commd, "exit", 4) == 0){
      commd_exit(client, commd);
    }
    else if(strncmp(commd, "ls", 2) == 0){
      commd_ls(client, commd);
    }
    else if(strncmp(commd, "get", 3) == 0){
      MusicData_Flag = 0;
      csound_audioInit();
      commd_get(client, commd);
    }
    else{
      printf("Command Is Error! Please Try Again!\n");
    }

    client.stop();
  }
  
  
}


/* 作用：打印帮助信息
 * 参数：
 *      无
 * 返回值：
 *      无
*/
void commd_help(void)
{
  Serial.printf("\n=------------------欢迎使用FTP------------------=\n");
  Serial.printf("|                                              |\n");
  Serial.printf("|           help: 显示所有FTP服务器命令           |\n");
  Serial.printf("|           exit: 离开FTP服务器                  |\n");
  Serial.printf("|           ls: 显示FTP服务器的文件列表            |\n");
  Serial.printf("|           get <file>: 从FTP服务器下载文件       |\n");
  Serial.printf("|                                              |\n");
  Serial.printf("|----------------------------------------------|\n");  
}

/* 作用：获取云端文件列表
 * 参数：
 *      client: TCP连接对象,类似于TCP文件描述符
 *      buffer: 数据保存位置
 * 返回值：
 *      无
*/
void commd_ls(WiFiClient client, char *buffer)
{
  if(client.write(buffer, N) < 0){
    printf("Write Error!\n");
    exit(1);
  }
  
  delay(20);//延时很重要，需要服务器那边将数据处理一下再发送

  //服务器那边多次发送，所以这里的延时要保证好，万一第二次服务器发送的时候,
  //超了一些时间，那么available函数就检测不到缓冲区有数据，直接跳过。
  while((client.connected() == 1) || (client.available()>0)){ 
    if(client.read((uint8_t *)buffer, N) > 0){
      Serial.printf("%s ", buffer);
    }
  }

  Serial.printf("\n");
  
}

/* 作用：下载文件
 * 参数：
 *      client: TCP连接对象,类似于TCP文件描述符
 *      buffer: 数据保存位置
 * 返回值：
 *      无
*/
void commd_get(WiFiClient client, char *buffer)
{
  size_t bytes_written;
  if(client.write(buffer, N) < 0){
    printf("Write Error!\n");
    exit(1);
  }

  delay(20);

  //判断服务器文件是否正常打开，既要考虑网络延迟，又不能多次读取
  while(client.available()>0){ // 用while却不是if主要考虑，如果因为网络延迟会导致缓冲区中数据为0，则直接跳过
      if(client.read((uint8_t *)buffer, N) > 0){
        Serial.printf("%s\n", buffer);
        
        if(buffer[0] != 'Y'){
          Serial.printf("Can't open this file!\n");
          exit(1);
        }
        else{
          break;  // 判断当前是第一次接收到，跳转出去，不然直接在该循环中读取文件内容，导致出现错误
        }
      }
      else{
        Serial.printf("Read error!\n");
        exit(1);
      }
      
  }

  Serial.printf("File content:\n");
  
  //开始接收文件
  while((client.connected() == 1) || (client.available()>0)){
    if(client.read((uint8_t *)buffer, N) > 0){
      //Serial.printf("%s ", buffer);
      play_wav(buffer);
    }
    
  }

  csound_audioDeinit();
  Serial.printf("\nFile recv end\n");
    
}



/* 播放WAV音频
 *
 */
static void play_wav(char *buffer)
{
    int i = 0;
    int wr_len = 0;
    size_t bytes_written;
    //判断是否是元数据，若是则解析元数据
    if(MusicData_Flag == 0){
        //寻找"data"字符串
        while(1){
          if(buffer[i] == 'd' && buffer[i+1] == 'a' && buffer[i+2] == 't' && buffer[i+3] == 'a'){
            buffer += 8;
            MusicData_Flag = 1;
            //Serial.printf("\n%s\n", buffer-8);
            break;
          }
          else{
            buffer++;
          }
        }
        
    }
    
    //8位数据扩展成16位数据
    wr_len = csound_audioDataScale((uint8_t *)i2s_wr_buffer, (uint8_t *)buffer, (uint32_t)(N-i));
	
    //音频数据，将通过I2S的方式发送给内部DAC
    i2s_write(I2S_NUM_0, i2s_wr_buffer, wr_len, &bytes_written, 100); 
        
}

int del_right(char *d_buff, char *s_buff, uint32_t len)
{
  uint32_t i = 0;
  uint32_t j = 0;
  for(i = 0, j = 0; i<len; i+=2){
    d_buff[j++] = s_buff[i++];
    d_buff[j++] = s_buff[i++];  
  }

  return (len/2);
}

/* 退出FTP
 *
*/
void commd_exit(WiFiClient client, char *buffer)
{
  if(client.write(buffer, N) < 0){
    printf("Write Error!\n");
    exit(1);
  }
  printf("Bye-bye! ^-^ \n");  //这接口兼容性，厉害
  Serial.printf("Device is disconnection, you can push RST to reconnect");
  while(1);//待优化：开启低功耗
}


/* 作用：获取串口字符串数据
 * 参数：
 *      buffer: 数据保存位置
 * 返回值：
 *      无
*/
void mySerial_Recv(char *buffer)
{
  int i = 0;
  while(1){
    if(Serial.available()){
      buffer[i] = Serial.read();
      if(buffer[i] == 10){
        break;
      }
      else{
        i++;
      }
    }
  }
  buffer[i] = '\0';
}


/* I2S初始化
 *
*/
static void csound_audioInit(void)
 {
     i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = 44100,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, //影响数据传输的格式，根据音频文件进行选择
      .communication_format = I2S_COMM_FORMAT_I2S_MSB,
      .intr_alloc_flags = 0,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false
     };

     
     i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL); //install and start i2s driver
     i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN); //可以控制有几个声道出声音
 }
 
 /* I2S卸载
 *
*/
 static void csound_audioDeinit()
 {
      i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
      i2s_driver_uninstall(I2S_NUM_0);
 }

 /* 作用：将8位数据转换为16位，这里要符合I2S中的DMA到DAC的规则，即16位数据，只取高8位输出到DAC,这里不要和PCM的弄混淆了
 *  参数：
 *      d_buff：转换后数据保存的起始地址
 *      s_buff：待转换数据的起始地址
 *      len：需要转换数据的字节数
 *  返回值：
 *      转换完成后所占用的字节数。
*/
static int csound_audioDataScale(uint8_t* d_buff, uint8_t* s_buff, uint32_t len)
{
  uint32_t j = 0;
  for (int i = 0; i < len; i++) {
    d_buff[j++] = 0;
    d_buff[j++] = s_buff[i];
  }
  return (len * 2);
}