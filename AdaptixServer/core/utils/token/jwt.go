package token

import (
	"crypto/rand"
	"encoding/base64"
	"errors"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
)

var accessKey string
var refreshKey string
var accessTokenLiveHours int
var refreshTokenLiveHours int

type Claims struct {
	Username string `json:"username"`
	jwt.RegisteredClaims
}

func InitJWT(accessTokenHours int, refreshTokenHours int) {
	accessKey, _ = generateRandomKey(32)
	refreshKey, _ = generateRandomKey(32)
	accessTokenLiveHours = accessTokenHours
	refreshTokenLiveHours = refreshTokenHours
}

func generateRandomKey(length int) (string, error) {
	bytes := make([]byte, length)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return base64.URLEncoding.EncodeToString(bytes), nil
}

func GenerateAccessToken(username string) (string, error) {
	expirationTime := time.Now().Add(time.Duration(accessTokenLiveHours) * time.Hour)
	claims := &Claims{
		Username: username,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(expirationTime),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString([]byte(accessKey))
	if err != nil {
		return "", err
	}
	return tokenString, nil
}

func GenerateRefreshToken(username string) (string, error) {
	expirationTime := time.Now().Add(time.Duration(refreshTokenLiveHours) * time.Hour)
	claims := &Claims{
		Username: username,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(expirationTime),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenString, err := token.SignedString([]byte(refreshKey))
	if err != nil {
		return "", err
	}
	return tokenString, nil
}

func GetUsernameFromJWT(token string) (string, error) {
	claims := &Claims{}
	jwtToken, err := jwt.ParseWithClaims(token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(accessKey), nil
	})

	if err != nil || !jwtToken.Valid {
		return "", errors.New("invalid token")
	}

	return claims.Username, nil
}

func ValidateAccessToken() gin.HandlerFunc {
	return func(ctx *gin.Context) {
		tokenString := ctx.GetHeader("Authorization")
		if tokenString == "" {
			_ = ctx.Error(errors.New("authorization token required"))
			ctx.Abort() // 必须调用Abort()终止请求链
			return
		}

		if strings.HasPrefix(tokenString, "Bearer ") {
			tokenString = tokenString[7:]
		} else {
			_ = ctx.Error(errors.New("authorization token required"))
			ctx.Abort() // 必须调用Abort()终止请求链
			return
		}

		username, err := GetUsernameFromJWT(tokenString)
		if err != nil {
			_ = ctx.Error(err)
			ctx.Abort() // 必须调用Abort()终止请求链
			return
		}

		ctx.Set("username", username)
		ctx.Next()
	}
}

func RefreshTokenHandler(ctx *gin.Context) {

	refreshToken := ctx.GetHeader("Authorization")
	if refreshToken == "" {
		_ = ctx.Error(errors.New("refresh token required"))
		ctx.Abort() // 必须调用Abort()终止请求链
		return
	}

	if strings.HasPrefix(refreshToken, "Bearer ") {
		refreshToken = refreshToken[7:]
	} else {
		_ = ctx.Error(errors.New("refresh token required"))
		ctx.Abort() // 必须调用Abort()终止请求链
		return
	}

	claims := &Claims{}
	token, err := jwt.ParseWithClaims(refreshToken, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(refreshKey), nil
	})

	if err != nil || !token.Valid {
		_ = ctx.Error(errors.New("invalid refresh token"))
		ctx.Abort() // 必须调用Abort()终止请求链
		return
	}

	newAccessToken, err := GenerateAccessToken(claims.Username)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate access token"))
		ctx.Abort() // 必须调用Abort()终止请求链
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"access_token": newAccessToken})
}
