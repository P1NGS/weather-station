///////////////////// 
// V2.4 08.12.2024 //
/////////////////////

////////////
// Header // 
////////////



//////////////
// Debugging//
//////////////

    #define DEBUG 0

    #if DEBUG == 1
    #define debug(x) Serial.print(x)
    #define debugln(x) Serial.println(x)
    #define debug_begin(x) Serial.begin(x)
    #else
    #define debug(x)
    #define debugln(x)
    #define debug_begin(x)
    #endif

///////////////
// Libraries //
///////////////

    #include <WiFi.h>                                     // WiFi
    #include <WiFiClientSecure.h>                         // Telegram
    #include <UniversalTelegramBot.h>                     // Telegram
    #include <InfluxDbClient.h>                           // InfluxDB
    #include <InfluxDbCloud.h>                            // InfluxDB
    #include <Adafruit_BMP280.h>                          // BMP280

//////////////////////////////////////
// Wifi network station credentials //
//////////////////////////////////////
    
    const char* WIFI_SSID     = "REDACTED";
    const char* WIFI_PASSWORD = "REDACTED";

/////////////
// Sensors //
/////////////

    ////////////
    // BMP280 //
    ////////////
        
        Adafruit_BMP280 bmp;                                    // BMP280 I2C 

        // BMP280 Sensor Data
        float   BMP_TEMPRATURE;                                 // BMP280
        float   BMP_PRESSURE;                                   // BMP280
        float   BMP_ALTITUDE;                                   // BMP280
    
    //////////
    // TMP36//
    //////////
        
        #define   g_TMP36_ReadSensors_MTBS 6000                    // TMP36 600000
        uint32_t  g_TMP36_ReadSensors_last_MTBS;                   // TMP36

        //////////////////////
        // Struct for TMP36 //
        //////////////////////
            
            struct TPM36_SensorStruct {
            int16_t tempC;
            uint8_t Pin;


            inline void setup(const uint8_t Pin) {
                pinMode(Pin, INPUT);
                this->Pin = Pin; // Store the pin number
            }
            
            void ReadSensors() {
                volatile uint16_t AvVolt= 0;
                uint16_t voltage;
                
                for (uint8_t i = 0; i < 16; i++){  
                AvVolt += analogRead(this->Pin);
                }
                
                voltage = ((AvVolt/16) * 1000) * (3.3 / 4095.0);
                this->tempC = map(voltage, 640, 660, 24, 26);
            }
            };

            // Sensor data instances
            TPM36_SensorStruct TMP36_sensor1;                         // TMP36 Inngang
            TPM36_SensorStruct TMP36_sensor2;                         // TMP36 Ute
            TPM36_SensorStruct TMP36_sensor3;                         // TMP36 benk
            TPM36_SensorStruct TMP36_sensor4;                         // TMP36 stue1


//////////////////
// Heat Element //
//////////////////

    struct HeaterControllerStruct {
        #define hyst_1_L 4
        #define hyst_1_H 6
        #define hyst_2_L 18
        #define hyst_2_H 22
        #define resetInterval 43200000  // 12 * 60 * 60 * 1000 // 12 timer

        uint8_t heatingState = 1;
        uint8_t Pin;
        int8_t Hlow = 0;
        int8_t Hup  = 2;
        uint8_t lastPinState = 0;
        uint32_t lastResetTime = 0;
        



        inline void setup_heatElement(const uint8_t Pin) {
            pinMode(Pin, OUTPUT);
            this->Pin = Pin; // Store the pin number
        }
        
        inline void resetHeatingStateIfNeeded() {
            uint32_t currentTime = millis();
            if ((currentTime - this->lastResetTime >= resetInterval) && (this->heatingState == 2)) {
                
                this->heatingState = 0;
                this->toggleHeating();
            }
        }
        
        inline void toggleHeating() {
            this->heatingState  = (this->heatingState + 1) % 3;
            this->Hlow          = (this->heatingState == 1 )  ? hyst_1_L : hyst_2_L;
            this->Hup           = (this->heatingState == 1)   ? hyst_1_H : hyst_2_H;
            
            // Update the last reset time
            if (this->heatingState == 2) {
                this->lastResetTime = millis();
            }
        }

        inline void heatElement(int16_t&  temp) {
            this->resetHeatingStateIfNeeded(); // Check if we need to reset the heating state
            
            uint8_t newPinState;

            if (heatingState == 0) {
                newPinState = LOW;

            } else if (heatingState == 1) {
                if (TMP36_sensor3.tempC < Hlow) {
                    newPinState = HIGH;
                } else if (TMP36_sensor3.tempC >= Hup) {
                    newPinState = LOW;
                }

            } else {
                if (temp < Hlow) {
                    newPinState = HIGH;
                } else if (temp >= Hup) {
                    newPinState = LOW;
                }
            }

            // Only update the pin state if it changes
            if (newPinState != lastPinState) {
                digitalWrite(Pin, newPinState);
                lastPinState = newPinState;
            }

        
        } 
    };

    HeaterControllerStruct HeaterController_Stue;   // heater Stue

///////////////////
/// Car battery ///
///////////////////
    
    struct CarBatteryStruct {
        uint8_t Pin;
        float volt;
        
        inline void setup(const uint8_t Pin) {
            pinMode(Pin, INPUT);
            this->Pin = Pin; // Store the pin number
        }


        inline void ReadVolt() {
            float AvVolt = 0.0;

            for (uint8_t i = 0; i < 16; i++) {
                AvVolt += analogRead(this->Pin); 
            }
            
            this->volt = ((AvVolt / 16.0) / 141.0) + 1.0; 
            
            debug("ADC: ");
            debugln(AvVolt / 16);
            debug("Volt: ");
            debugln(this->volt);
        }


    };

    CarBatteryStruct CarBattery_Benk;       // Car_battery Benk


/////////////////////////////////////////////
/////////// Alerts to telegram //////////////
/////////////////////////////////////////////

    #define   g_Alerts_MTBS 10800000           // mean time between between alerts scan
    uint32_t  g_alerts_last_MTBS;              // last time alerts' scan has been done    


/////////////////////////////////////////////
// Telegram BOT Token (Get from Botfather) //
/////////////////////////////////////////////
    
    #define BOT_TOKEN "REDACTED"
    #define CHAT_ID "REDACTED"

    WiFiClientSecure secured_client;
    UniversalTelegramBot bot(BOT_TOKEN, secured_client);

    #define   g_BOT_MTBS 4000           // mean time between scan messages
    uint32_t  g_bot_lasttime;           // last time messages' scan has been done
////////////////////
// InfluxDB Cloud //
////////////////////
    
    #define INFLUXDB_URL    "REDACTED"
    #define INFLUXDB_TOKEN  "REDACTED"
    #define INFLUXDB_ORG    "REDACTED"
    #define INFLUXDB_BUCKET "REDACTED"
    #define TZ_INFO         "REDACTED"
    #define DEVICE          "ESP32"

    #define   g_InfluxDB_MTBS 10800000 // 3 hours
    uint32_t  g_InfluxDB_last_MTBS;

    InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
    Point sensor("Sensor_Data");





//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  void setup()  *///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void setup()
    {
    setCpuFrequencyMhz(160); // Set CPU clock frequency to 160 MHz from 240 MHz
    debug_begin(112500);
    
    debugln("WeatherStation");
    debugln(" ");
    
    // default settings
    setup_wifi();
    setup_InfluxDB();
    
    // GIOPINS
    HeaterController_Stue.setup_heatElement(12);
    CarBattery_Benk.setup(32);
    TMP36_sensor1.setup(A2);
    TMP36_sensor2.setup(A3);
    TMP36_sensor3.setup(A4);
    TMP36_sensor4.setup(33);
    
    
    
    
    bmp.begin();       // BMP280
    /* BMP280_1 BMP280_2 Default settings from datasheet. */ 
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,    /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* BMP_PRESSURE oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

    }



//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  void loop()  *////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void loop()
    {
    delay(100); // Less stress on prossesor


    // Run func
    if (millis() - g_TMP36_ReadSensors_last_MTBS > g_TMP36_ReadSensors_MTBS)    ReadSensors();
    if (millis() - g_alerts_last_MTBS > g_Alerts_MTBS)                          Alerts();
    if (millis() - g_InfluxDB_last_MTBS > g_InfluxDB_MTBS)                      InfluxDB();
    HeaterController_Stue.heatElement(TMP36_sensor4.tempC);


    // Telegram
    if (millis() - g_bot_lasttime > g_BOT_MTBS)
    {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages)
        {
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        g_bot_lasttime = millis();
    }

    }



//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  setup_wifi  */////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void setup_wifi() {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
        
        debug("Connecting to Wifi SSID ");
        debugln(WIFI_SSID);
        debugln(WIFI_PASSWORD);
        debugln();
        debug("Connecting to ");
        
        
        while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        debug(".");
        }

        debug("WiFi connected - ESP IP address: ");
        debugln(WiFi.localIP());

    }



//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  setup_InfluxDB  */////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void setup_InfluxDB() {
        debugln("Conecting to InfluxDB");
        
        sensor.addTag("device", DEVICE);
        timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
        
        //check conection
        if (!client.validateConnection()) {
        debug("InfluxDB Connection Failed: ");
        debugln(client.getLastErrorMessage());
        return;
        }
    }



//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  InfluxDB  *///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void InfluxDB() {

    if (millis() - g_InfluxDB_last_MTBS > g_InfluxDB_MTBS)
    {
        
        if (!client.validateConnection()) {
        
        debug("InfluxDB Connection Failed: ");
        debugln(client.getServerUrl());
        
        g_InfluxDB_last_MTBS = millis();
        return; // Exit the function if InfluxDB connection fails
        }

        // Clear fields for reusing the point. Tags will remain the same as set above.
        timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
        sensor.clearFields();
        
        // Add sensor data
        sensor.addField("TMP36_inngang",    TMP36_sensor1.tempC);       // TMP36 readings
        sensor.addField("TMP36_uteT",       TMP36_sensor2.tempC);       // TMP36 readings
        sensor.addField("TMP36_benkT",      TMP36_sensor3.tempC);       // TMP36 readings
        sensor.addField("TMP36_stueT",      TMP36_sensor4.tempC);       // TMP36 readings
        sensor.addField("CarBat_benkV",     CarBattery_Benk.volt);      // CarBattery_Benk Voltage level
        sensor.addField("BMP_esp",          BMP_TEMPRATURE);            // BMP280_1 BMP_PRESSURE in hPa
        sensor.addField("BMP_mBar",         BMP_PRESSURE);              // BMP280_1 BMP_PRESSURE in hPa
        sensor.addField("BMP_hoyde",        BMP_ALTITUDE);              // BMP280_1 BMP_ALTITUDE in meters

        // Write point
        if (!client.writePoint(sensor)) {
        debug("InfluxDB write failed: ");
        debugln(client.getLastErrorMessage());
        }

        g_InfluxDB_last_MTBS = millis();
    }
    }



//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  ReadSensors  *////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void ReadSensors() 
    {
        g_TMP36_ReadSensors_last_MTBS = millis();                 // Rest Timer

        BMP_TEMPRATURE  = bmp.readTemperature();                    // BMP280 BMP_TEMPRATURE in Celcius
        BMP_PRESSURE    = bmp.readPressure() / 100.0;               // BMP280 BMP_PRESSURE in hPa
        BMP_ALTITUDE    = bmp.readAltitude(1000);                   // BMP280 BMP_ALTITUDE in meters


        TMP36_sensor1.ReadSensors();                                // TPM36 Reads TMP_TEMPRATURE: 
        TMP36_sensor2.ReadSensors();                                // TPM36 Reads TMP_TEMPRATURE: 
        TMP36_sensor3.ReadSensors();                                // TPM36 Reads TMP_TEMPRATURE:
        TMP36_sensor4.ReadSensors();                                // TPM36 Reads TMP_TEMPRATURE:
        
        CarBattery_Benk.ReadVolt();                                 // CarBattery_Benk Reads Volt: 
    }



//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  Alerts to telegram  *////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    
    void Alerts() 
    {
        g_alerts_last_MTBS = millis();
        
        // Alerts when car battery get to low
        if (CarBattery_Benk.volt < 12.6) {
            bot.sendMessage(CHAT_ID, "Advaserl arBattery_Benk er: " + String(CarBattery_Benk.volt) + " V", "");
            debug("Warn Low Volt: ");
            debugln(CarBattery_Benk.volt);
        }

        // Alerts when benk gets to cold
        if (TMP36_sensor3.tempC < 5) {
            bot.sendMessage(CHAT_ID, "Advaserl bmp sensor er: " + String(TMP36_sensor3.tempC) + " C", "");
            debug("Warn Low Temp: ");
            debugln(TMP36_sensor3.tempC);
        }
    
    }

//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  TicTacToe  *//////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

    void TicTacToe(const String& command, const String& chat_id) {
        struct StructGame {
            char player = 'X';
            int turn = 0;
            char board[3][3] = {
                { ' ', ' ', ' ' },
                { ' ', ' ', ' ' },
                { ' ', ' ', ' ' }
            };

            void reset() {
                turn = 0;
                player = 'X';
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                        board[i][j] = ' ';
                    }
                }
            }

            void GamePrint(const String& output, const String& chat_id) {
                bot.sendMessage(chat_id, output, "");
            }

            void drawBoard(String& output) {
                output += "-------------\n";
                for (int i = 0; i < 3; i++) {
                    output += "| ";
                    for (int j = 0; j < 3; j++) {
                        output += board[i][j];
                        output += " | ";
                    }
                    output += "\n-------------\n";
                }
            }

            bool checkWin(char player) {
                for (int i = 0; i < 3; i++) {
                    if (board[i][0] == player && board[i][1] == player && board[i][2] == player)
                        return true;
                    if (board[0][i] == player && board[1][i] == player && board[2][i] == player)
                        return true;
                }
                if (board[0][0] == player && board[1][1] == player && board[2][2] == player)
                    return true;
                if (board[0][2] == player && board[1][1] == player && board[2][0] == player)
                    return true;
                return false;
            }
        };

        static StructGame Game;
        String output;

        // Check if the command is to print the board
        if (command == "/Tic") {
            Game.drawBoard(output);
            Game.GamePrint(output, chat_id);
            return;
        }

        // Otherwise, it should be a move command
        if (command.startsWith("/Tic_") && command.length() == 7) {
            int x = command[5] - '0'; // Horizontal coordinate
            int y = command[6] - '0'; // Vertical coordinate

            if (x < 0 || x > 2 || y < 0 || y > 2 || Game.board[y][x] != ' ') {
                output += "Invalid move. Try again.\n";
                Game.GamePrint(output, chat_id);
                return;
            }

            Game.board[y][x] = Game.player; // Note the swapped coordinates
            Game.drawBoard(output);

            if (Game.checkWin(Game.player)) {
                output += "Player ";
                output += Game.player;
                output += " wins!\n";
                Game.reset();
                Game.GamePrint(output, chat_id);
                return;
            }

            if (++Game.turn == 9) {
                output += "It's a draw!\n";
                Game.reset();
                Game.GamePrint(output, chat_id);
                return;
            }

            // Switch player
            Game.player = (Game.player == 'X') ? 'O' : 'X';
        }

        Game.GamePrint(output, chat_id);
    }







//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  BlackJack  *//////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

    void BlackJack(const String& command, const String& chat_id) {
        
        struct Card {
            char rank;
            char suit;
        };

        // Define the struct inside the function
        struct DeckStruct {
            char deck_list[104]; 
            uint8_t index; // show index of top card in deck_list
            uint8_t deck_empty = 1;

            // Constructor to initialize index
            DeckStruct() : index(104), deck_empty(1) {} // Initialize index to the end of the deck

            void Shuffle() {
                /*
                    @return void
                    shuffles the deck with a simple algorithm
                    sets the deck_list to a shuffled order and stores both rank and suit

                    ##### #### #### #### #### #### #### #### #### #### #### #### #### #### #### 
                    all functions from DeckStruct
                    static DeckStruct deck;                 // Static to ensure it's initialized only once
                    deck.Shuffle();                         // shuffle deck but only if deck.deck_empty is 1
                    Card lastCard = deck.deck_get_card();   // rm top card from deck and ads the card to lastCard.rank and lastCard.suit
                    deck.deck_println();                    // SerialPrint the deck
                */

                // Initialize the deck with cards
                if (this->deck_empty) {
                    // Initialize the deck with cards
                    const char ranks[13] = {'2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'};
                    const char suits[4] = {'H', 'D', 'C', 'S'};

                    this->index = 0; // Use the class member index
                    for (uint8_t i = 0; i < 13; i++) {
                    for (uint8_t j = 0; j < 4; j++) {
                        this->deck_list[this->index++] = ranks[i]; 
                        this->deck_list[this->index++] = suits[j]; 
                    }
                    }

                    for (uint8_t i = 0; i < 103; i += 2) {
                        // Randomly select a card to swap with
                        uint8_t ran = random(0, 51) * 2;
                        
                        // Swap cards at positions i and r
                        char temp_rank              = this->deck_list[i];
                        char temp_suit              = this->deck_list[i + 1];
                        this->deck_list[i]          = this->deck_list[ran];
                        this->deck_list[i + 1]      = this->deck_list[ran + 1];
                        this->deck_list[ran]        = temp_rank;
                        this->deck_list[ran + 1]    = temp_suit;
                    }
                    // Update deck_empty flag
                    this->deck_empty = 0;
                }
            }

            Card deck_get_card() {
            /*
                @return Card
                returns the last card in deck_list (Rank, Suit)
                removes the card from the deck
            */
            Card card;

            // Ensure there are cards left in the deck
            if (this->index > 0) {
                // Retrieve the rank and suit of the last card
                card.rank = deck_list[this->index - 2];
                card.suit = deck_list[this->index - 1];

                // Decrement the index to remove the card from the deck
                this->index -= 2;
            } else {
                // shuffle the cards when index is 0 (deck is empty)
                this->deck_empty = 1;
                Shuffle();

                // Retrieve the rank and suit of the last card
                card.rank = deck_list[this->index - 2];
                card.suit = deck_list[this->index - 1];

                // Decrement the index to remove the card from the deck
                this->index -= 2;
            }

            // Return the card struct
            return card;
            }
        };



        /*
            @ Struct
            @ for Player and House
            #
            Varibles:
            - int Player_card_value
            - int index
            - char player_card_hand[14]
            - String Print_Game
        */ 
        struct Hands {
        uint8_t Player_card_value = 0;
        uint8_t index = 0;
        char player_card_hand[11];

        uint8_t Dealer_card_value = 0;
        uint8_t Dealer_index = 0;
        char Dealer_card_hand[11];

        uint16_t Moeny = 1024;
        uint8_t PrintWinner = 1;
        String Print_Game;

        void NewRound(DeckStruct& deck) {
            deck.deck_empty = 1;
            deck.Shuffle();

            this->PrintWinner = 0;

            this->Player_card_value = 0;
            this->index = 0;
            memset(this->player_card_hand, 0, sizeof(this->player_card_hand));

            this->Dealer_card_value = 0;
            this->Dealer_index = 0;
            memset(this->Dealer_card_hand, 0, sizeof(this->Dealer_card_hand));

            DealerDrawCard(deck);
            DealerDrawCard(deck);
            DealerAction_One();

            PlayerDrawCard(deck);
            PlayerDrawCard(deck);
        }

        void DealerDrawCard(DeckStruct& deck) {
            Card drawnCard = deck.deck_get_card();
            this->Dealer_card_hand[this->Dealer_index++] = drawnCard.rank;
            this->Dealer_card_hand[this->Dealer_index++] = drawnCard.suit;

            switch (drawnCard.rank) {
                case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                    this->Dealer_card_value += drawnCard.rank - '0';
                    break;
                case 'T': case 'J': case 'Q': case 'K':
                    this->Dealer_card_value += 10;
                    break;
                case 'A':
                    this->Dealer_card_value += 11;
                    break;
            }
        }

        void DealerAction_One() {
            this->Print_Game += "\nDealer Hand: ";
            this->Print_Game += this->Dealer_card_hand[0];
            this->Print_Game += this->Dealer_card_hand[1];
        }

        void DealerAction_Two(DeckStruct& deck) {
            for (; this->Dealer_card_value < this->Player_card_value || (this->Dealer_card_value <= 21 && this->Dealer_card_value <= this->Player_card_value);) {
                DealerDrawCard(deck);
            }
            DealerPrintHand();
        }

        void DealerPrintHand() {
            this->Print_Game = "\nDealer Hand: ";
            for (uint8_t i = 0; i < this->Dealer_index; i += 2) {
                this->Print_Game += this->Dealer_card_hand[i];
                this->Print_Game += this->Dealer_card_hand[i + 1];
                this->Print_Game += " ";
            }
        }

        void PlayerPrintHand() {
            this->Print_Game += "\nYour Hand: ";
            for (uint8_t i = 0; i < this->index; i += 2) {
                this->Print_Game += this->player_card_hand[i];
                this->Print_Game += this->player_card_hand[i + 1];
                this->Print_Game += " ";
            }
        }

        void PlayerDrawCard(DeckStruct& deck) {
            Card drawnCard = deck.deck_get_card();
            this->player_card_hand[this->index++] = drawnCard.rank;
            this->player_card_hand[this->index++] = drawnCard.suit;

            switch (drawnCard.rank) {
                case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                    this->Player_card_value += drawnCard.rank - '0';
                    break;
                case 'T': case 'J': case 'Q': case 'K':
                    this->Player_card_value += 10;
                    break;
                case 'A':
                    this->Player_card_value += 11;
                    break;
            }
        }

        uint8_t PlayerAction(const String& command, DeckStruct& deck) {
            if (Moeny > 0) {
                if ((command == "/BJ_hit") && this->Player_card_value <= 21 && !this->PrintWinner) {
                    this->Print_Game = "\nDealer Hand: ";
                    this->Print_Game += this->Dealer_card_hand[0];
                    this->Print_Game += this->Dealer_card_hand[1];

                    PlayerDrawCard(deck);
                    return 1;

                } else if (command == "/BJ_stand" && !this->PrintWinner) {
                    this->PrintWinner = 1;
                    DealerAction_Two(deck);
                    return 1;

                } else if (command == "/BJ_start" && this->PrintWinner) {
                    this->Print_Game = "Game Commands:\n/BJ_start\n/BJ_hit\n/BJ_stand\n";
                    NewRound(deck);
                    return 1;

                } else {
                    return 0;
                }
            } else if (command == "/BJ_hit" || command == "/BJ_stand" || command == "/BJ_start") {
                this->Print_Game = "Not Enough Money";
                return 1;
            }
        }

        void PrintGame(const String& chat_id) {
            PlayerPrintHand();

            if ((Player_card_value <= 21 && (Player_card_value > Dealer_card_value || Dealer_card_value > 21)) && (this->PrintWinner)) {
                Print_Game += "\nYou Win!";
                this->Moeny += 16;
                Print_Game += "\nMoney:";
                Print_Game += Moeny;


            } else if (this->PrintWinner) {
                Print_Game += "\nYou Lost!";
                this->Moeny -= 16;
                Print_Game += "\nMoney:";
                Print_Game += Moeny;

            }

            bot.sendMessage(chat_id, Print_Game, "");
        }
    };

    
    // main game loop
    static DeckStruct deck;
    static Hands player;

    if (player.PlayerAction(command, deck)) {
        player.PrintGame(chat_id);
    }
    // end of main game loop
}
    





//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////*  Telegram  *///////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
    void handleNewMessages(int numNewMessages)
    {
        for (int i = 0; i < numNewMessages; i++)
        {
            String chat_id = String(bot.messages[i].chat_id);
            if (chat_id != CHAT_ID )
            {
                bot.sendMessage(chat_id, "Unauthorized", "");
            }
            else {
                // Declare the welcome string outside the switch statement
                const String welcome =
                    "Værstasjons informasjon\n"
                    "/Stue1      : Temperature TMP36\n"
                    "/inngang    : Temperature TMP36\n"
                    "/ute        : Temperature TMP36\n"
                    "/benk       : Temperature TMP36\n"
                    "/BMPesp     : Temperature BMP\n"
                    "/mBar       : Pressure BMP\n"
                    "/hoyde      : Altitude BMP\n"
                    "/bilbatteri : bilbatteri Benk\n"
                    "/ovnStue    : Varmeovn Stue\n"
                    "--Ovn: 0 = AV\n"
                    "--Ovn: 1 = 4->6\n"
                    "--Ovn: 2 = 18->22\n";
                
                String command = bot.messages[i].text;

                if (command == "/inngang") {
                    bot.sendMessage(chat_id, String(TMP36_sensor1.tempC) + " C", "");
                    return;
                }
                if (command == "/stue1") {
                    bot.sendMessage(chat_id, String(TMP36_sensor4.tempC) + " C", "");
                    return;
                }
                else if (command == "/ute") {
                    bot.sendMessage(chat_id, String(TMP36_sensor2.tempC) + " C", "");
                    return;
                }
                else if (command == "/benk") {
                    bot.sendMessage(chat_id, String(TMP36_sensor3.tempC) + " C", "");
                    return;
                }
                else if (command == "/BMPesp") {
                    bot.sendMessage(chat_id, String(BMP_TEMPRATURE) + " C", "");
                    return;
                }
                else if (command == "/mBar") {
                    bot.sendMessage(chat_id, String(BMP_PRESSURE) + " mBar", "");
                    return;
                }
                else if (command == "/hoyde") {
                    bot.sendMessage(chat_id, String(BMP_ALTITUDE) + " M", "");
                    return;
                }
                else if (command == "/ovnStue") {
                    HeaterController_Stue.toggleHeating();
                    bot.sendMessage(chat_id, "Ovn:" + String(HeaterController_Stue.heatingState), "");
                    return;
                }
                else if (command == "/bilbatteri") {
                    bot.sendMessage(chat_id, "bilbatteri Volt: " + String(CarBattery_Benk.volt) + " V", "");
                    return;
                }
                else if (command == "/start") {
                    bot.sendMessage(chat_id, welcome, "Markdown");
                    return;
                }
                else {
                    TicTacToe(command, chat_id);
                    BlackJack(command, chat_id);
                    return;
                } 
            
            }
        }
    }
    
