package utils

import (
	"encoding/base64"
	"encoding/json"

	"github.com/pion/webrtc/v4"
)

// JSON encode + base64 a SessionDescription
func Encode(obj *webrtc.SessionDescription) string {
	b, err := json.Marshal(obj)
	if err != nil {
		panic(err)
	}

	return base64.StdEncoding.EncodeToString(b)
}

// Decode a base64 and unmarshal JSON into a SessionDescription
func Decode(in ConnectObj, obj *webrtc.SessionDescription) {
	b, err := base64.StdEncoding.DecodeString(in.Sdp)
	if err != nil {
		panic(err)
	}

	if err = json.Unmarshal(b, obj); err != nil {
		panic(err)
	}
}
