package main

import (
	"crypto/md5"
	"encoding/json"
	"fmt"
	"log"
	"time"

	"github.com/go-redis/redis"
	"github.com/gorilla/websocket"
)

// TestDefinition - Test definition
type TestDefinition struct {
	name         string
	numClients   int
	payloadSize  int
	iterations   int
	redisAddr    string
	redisChannel string
	eventhubURL  string
	timeout      int
}

// EventhubClient - Client
type EventhubClient struct {
	id     int
	wsConn *websocket.Conn
	responseTimes []time.Duration
}

type testPacket struct {
	ID        int       `json:"id"`
	Timestamp time.Time `json:"timestamp"`
	Payload   string    `json:"payload"`
	Checksum  string    `json:"checksum"`
}

func calcResponseTimes(responseTimes *[]time.Duration) (min time.Duration, max time.Duration, avg time.Duration) {
	for _, r := range *responseTimes {
		avg = avg + r

		if min == 0 || min > r {
			min = r
		}

		if max == 0 || max < r {
			max = r
		}
	}

	avg = avg / time.Duration(len(*responseTimes))

	return min, max, avg
}

// Run - Run client
func (c *EventhubClient) Run(test *TestDefinition, ch *chan *EventhubClient) {
	var err error
	c.wsConn, _, err = websocket.DefaultDialer.Dial(test.eventhubURL, nil)

	if err != nil {
		log.Fatalf("Client %d failed to connect to eventhub: %v", c.id, err)
	}

	defer c.wsConn.Close()

	*ch <- c

	for {
		_, message, err := c.wsConn.ReadMessage()
		if err != nil {
			log.Fatalf("Client %d read error: %v", c.id, err)
			return
		}

		var recvPacket testPacket
		err = json.Unmarshal(message, &recvPacket)
		if err != nil {
			log.Fatalf("Client %d failed to unmarshal message: %v\nMessage: '%s'", c.id, err, message)
		}

		payloadCheckSum := fmt.Sprintf("%x", md5.Sum([]byte(recvPacket.Payload)))
		if payloadCheckSum != recvPacket.Checksum {
			log.Fatalf("Client %d failed to validate payload cheksum: expected '%s' was '%s'", c.id, recvPacket.Checksum, payloadCheckSum)
		}

		diff := time.Now().Sub(recvPacket.Timestamp)
		c.responseTimes = append(c.responseTimes, diff)

		if len(c.responseTimes) == test.iterations {
			*ch <- c
			return
		}
	}
}

// ExecuteTest Executes a TestDefinition
func ExecuteTest(test *TestDefinition) bool {
	redisClient := redis.NewClient(&redis.Options{
		Addr:     test.redisAddr,
		Password: "",
		DB:       0,
	})

	defer redisClient.Close()


	// Wait for all clients to connect.
	log.Printf("Waiting for all %d clients to connect.", test.numClients)

	// Create clients.
	var clients []EventhubClient
	ch := make(chan *EventhubClient)
	for i := 0; i < test.numClients; i++ {
		clients = append(clients, EventhubClient{id: i})
		go clients[i].Run(test, &ch)
		time.Sleep(1 * time.Millisecond)
	}

	nConnected := 0
	for {
		<-ch
		nConnected++

		if nConnected == test.numClients {
			log.Printf("All %d clients connected.", test.numClients)
			break
		}

		// Todo: Implement timeout.
	}

	// Publish messages.
	go func (){
		for i := 0; i < test.iterations; i++ {
			// Generate payload string.
			var payload string
			for i := 0; i < test.payloadSize; i++ {
				payload = payload + "*"
			}

			payloadCheckSum := fmt.Sprintf("%x", md5.Sum([]byte(payload)))

			packet := testPacket{
				ID:        i,
				Timestamp: time.Now(),
				Payload:   payload,
				Checksum:  payloadCheckSum,
			}

			jsonPacket, err := json.Marshal(packet)
			if err != nil {
				log.Fatalf("Failed to create marshal packet to JSON: %v", err)
			}

			redisClient.Publish(test.redisChannel, string(jsonPacket))
			//log.Printf("Publish: %s -> %s\n\n", test.redisChannel, string(jsonPacket))
		}
	}()

	// Wait for all clients to finish.
	nFinished := 0

	var responseTimes []time.Duration

	for {
		select {
		case client := <-ch:
			nFinished++

			//log.Printf("Client %d: min: %s max: %s avg: %s", 
			//client.id, client.results.minResponseTime.String(), client.results.maxResponseTime.String(), client.results.calcAvgResponseTime().String())

			for _, r := range client.responseTimes {
				responseTimes = append(responseTimes, r)
			}

			if nFinished == nConnected {
				fmt.Printf("\n")
				log.Printf("All clients finished successfully.")
				log.Printf("Summary of %d published messages spread across %d clients:", len(responseTimes), nFinished)

				min, max, avg := calcResponseTimes(&responseTimes)

				log.Printf("Min: %s Max: %s Avg: %s", 
					min.String(), max.String(), avg.String())
				return true
			}
		}
	}
}

func main() {
	test1 := TestDefinition{
		name:         "Test 1",
		numClients:  	20000,
		payloadSize:  1000,
		iterations:   10,
		redisChannel: "eventhub.test1",
		redisAddr:    "127.0.0.1:6379",
		eventhubURL:  "ws://127.0.0.1:8080/test1",
	}

	ExecuteTest(&test1)
}
