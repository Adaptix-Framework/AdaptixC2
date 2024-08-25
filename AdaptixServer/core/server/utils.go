package server

import (
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
)

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	AdaptixServer *httphandler.TsHttpHandler

	clients safe.Map
}
