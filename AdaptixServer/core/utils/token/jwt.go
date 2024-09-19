package token

import (
	"crypto/rand"
	"encoding/base64"
	"errors"
	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt"
	"net/http"
	"strings"
	"time"
)

var accessKey string
var refreshKey string

type Claims struct {
	Username string `json:"username"`
	jwt.StandardClaims
}

func InitJWT() {
	accessKey, _ = generateRandomKey(32)
	refreshKey, _ = generateRandomKey(32)
}

func generateRandomKey(length int) (string, error) {
	bytes := make([]byte, length)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return base64.URLEncoding.EncodeToString(bytes), nil
}

func GenerateAccessToken(username string) (string, error) {
	expirationTime := time.Now().Add(8 * time.Hour)
	claims := &Claims{
		Username: username,
		StandardClaims: jwt.StandardClaims{
			ExpiresAt: expirationTime.Unix(),
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
	expirationTime := time.Now().Add(7 * 24 * time.Hour)
	claims := &Claims{
		Username: username,
		StandardClaims: jwt.StandardClaims{
			ExpiresAt: expirationTime.Unix(),
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
			return
		}

		if strings.HasPrefix(tokenString, "Bearer ") {
			tokenString = tokenString[7:]
		} else {
			_ = ctx.Error(errors.New("authorization token required"))
			return
		}

		username, err := GetUsernameFromJWT(tokenString)
		if err != nil {
			_ = ctx.Error(err)
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
		return
	}

	claims := &Claims{}
	token, err := jwt.ParseWithClaims(refreshToken, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(refreshKey), nil
	})

	if err != nil || !token.Valid {
		_ = ctx.Error(errors.New("invalid refresh token"))
		return
	}

	newAccessToken, err := GenerateAccessToken(claims.Username)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate access token"))
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"access_token": newAccessToken})
}
