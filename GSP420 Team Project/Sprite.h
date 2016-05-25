#pragma once
#include "Texture.h"
#include "Utility.h"

class Sprite {
public:
	void setBitmap(Texture& texture);
	void draw();

	float opacity= 1.0f;
	GSPRect srcRect;
	GSPRect destRect;
  int z = 0;

private:
	CComPtr<ID2D1Bitmap> bmp;

	friend class Graphics;
  //set this in bootstrap
  static Graphics* gfx;

};