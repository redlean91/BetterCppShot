#include "CompositeScreenshot.h"
#include "../Utils.h"

#include <stdexcept>
#include <cstring>
#include <sstream>

inline BYTE toByte(int value){
    return value > 255 ? 255 : value;
}

void CompositeScreenshot::init(const Screenshot& white, const Screenshot& black){
	Gdiplus::Bitmap* whiteShot = white.getBitmap(), *blackShot = black.getBitmap();
	if(whiteShot->GetWidth() != blackShot->GetWidth() || whiteShot->GetHeight() != blackShot->GetHeight()) throw std::runtime_error("Black/white screenshot size mismatch");
    if(whiteShot->GetWidth() == 0 || whiteShot->GetHeight() == 0) throw std::runtime_error("Zero width captured screenshot");

	m_image = new Gdiplus::Bitmap(whiteShot->GetWidth(), whiteShot->GetHeight(), PixelFormat32bppARGB);
    m_captureRect = white.getCaptureRect();

	differentiateAlpha(whiteShot, blackShot);
	if (!m_noCrop) cropImage();
}

CompositeScreenshot::CompositeScreenshot(const Screenshot& white, const Screenshot& black) : Screenshot() {
    this->init(white, black);
}

CompositeScreenshot::CompositeScreenshot(const Screenshot& white, const Screenshot& black, Gdiplus::Rect crop) : Screenshot() {
    m_crop = crop;
    this->init(white, black);
}

CompositeScreenshot::CompositeScreenshot(const Screenshot& white, const Screenshot& black, Gdiplus::Rect crop, bool blackOpaque) : Screenshot() {
    m_crop = crop;
    m_blackOpaque = blackOpaque;
    
    // Detect Windows version for mask threshold
    OSVERSIONINFO osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&osvi)) {
        bool isWindows11Plus = (osvi.dwMajorVersion >= 10);
        m_minAlpha = isWindows11Plus ? 254 : 0;  // Vista and Win7 use 0, Win11+ uses 254
    }
    
    this->init(white, black);
}

CompositeScreenshot::CompositeScreenshot(const Screenshot& white, const Screenshot& black, bool noCrop) : Screenshot() {
    m_noCrop = noCrop;
    this->init(white, black);
}

CompositeScreenshot::CompositeScreenshot(const Screenshot& white, const Screenshot& black, bool noCrop, bool blackOpaque) : Screenshot() {
    m_noCrop = noCrop;
    m_blackOpaque = blackOpaque;
    
    // Detect Windows version for mask threshold
    // Vista and Windows 7 use minAlpha=0, Windows 11+ uses minAlpha=254
    OSVERSIONINFO osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&osvi)) {
        bool isWindows11Plus = (osvi.dwMajorVersion >= 10);
        m_minAlpha = isWindows11Plus ? 254 : 0;
    }
    
    this->init(white, black);
}

void CompositeScreenshot::differentiateAlpha(Gdiplus::Bitmap* whiteShot, Gdiplus::Bitmap* blackShot){
    auto monitorRects = CppShot::getMonitorRects();

	Gdiplus::BitmapData transparentBitmapData;
    Gdiplus::Rect rect1(0, 0, m_image->GetWidth(), m_image->GetHeight());
    m_image->LockBits(&rect1, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &transparentBitmapData);
    BYTE* transparentPixels = (BYTE*) transparentBitmapData.Scan0;

    Gdiplus::BitmapData whiteBitmapData;
    whiteShot->LockBits(&rect1, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &whiteBitmapData);
    const BYTE* whitePixels = (BYTE*) whiteBitmapData.Scan0;

    Gdiplus::BitmapData blackBitmapData;
    blackShot->LockBits(&rect1, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &blackBitmapData);
    const BYTE* blackPixels = (BYTE*) blackBitmapData.Scan0;

    bool isOnlyOneMonitorConnected = m_noCrop || monitorRects.size() == 1;

    auto width = whiteShot->GetWidth();
    auto height = whiteShot->GetHeight();

    for(int y = 0; y < height; y++){
        BYTE* transparentRow = transparentPixels + y * transparentBitmapData.Stride;
        const BYTE* whiteRow = whitePixels + y * whiteBitmapData.Stride;
        const BYTE* blackRow = blackPixels + y * blackBitmapData.Stride;
        BYTE* transparentFullBegin = nullptr;
        const BYTE* whiteFullBegin = nullptr;

        for(int x = 0; x < width; x++){
            int currentPixel = x * 4;

            bool isInsideMonitor = isOnlyOneMonitorConnected;
            if(!isInsideMonitor){
                for(auto monitorRect : monitorRects){
                    if(x + m_captureRect.left >= monitorRect.left && x+ m_captureRect.left < monitorRect.right && y + m_captureRect.top >= monitorRect.top && y + m_captureRect.top < monitorRect.bottom){
                        isInsideMonitor = true;
                        break;
                    }
                }
            }

            // Oddly enough this makes the code both faster and more readable
            // compared to direct array accesses in the calculation itself
            BYTE blackR = blackRow[currentPixel + 2];
            BYTE blackG = blackRow[currentPixel + 1];
            BYTE blackB = blackRow[currentPixel];
            BYTE whiteR = whiteRow[currentPixel + 2];
            BYTE whiteG = whiteRow[currentPixel + 1];
            BYTE whiteB = whiteRow[currentPixel];

            // Calculate alpha
            BYTE alpha = isInsideMonitor
                ? toByte((blackR - whiteR + 255 + blackG - whiteG + 255 + blackB - whiteB + 255) / 3)
                : 0;

            if (m_blackOpaque) {
                if (alpha > m_minAlpha) {
                    transparentRow[currentPixel + 3] = 255;
                    transparentRow[currentPixel + 2] = 0;
                    transparentRow[currentPixel + 1] = 0;
                    transparentRow[currentPixel] = 0;
                } else {
                    transparentRow[currentPixel + 3] = 0;
                    transparentRow[currentPixel + 2] = 0;
                    transparentRow[currentPixel + 1] = 0;
                    transparentRow[currentPixel] = 0;
                }
            } else if (alpha == 255) {
                if(transparentFullBegin == nullptr) transparentFullBegin = transparentRow + currentPixel;
                if(whiteFullBegin == nullptr) whiteFullBegin = whiteRow + currentPixel;
            } else {
                if(transparentFullBegin != nullptr) {
                    std::memcpy(transparentFullBegin, whiteFullBegin, (transparentRow + currentPixel) - transparentFullBegin);
                    transparentFullBegin = nullptr;
                    whiteFullBegin = nullptr;
                }

                if (alpha > 0) {
                    transparentRow[currentPixel + 3] = alpha;
                    transparentRow[currentPixel + 2] = toByte(255 * blackR / alpha); // RED
                    transparentRow[currentPixel + 1] = toByte(255 * blackG / alpha); // GREEN
                    transparentRow[currentPixel] = toByte(255 * blackB / alpha); // BLUE
                }
            }
        }

        if (transparentFullBegin != nullptr)
        {
            std::memcpy(transparentFullBegin, whiteFullBegin,
                        (transparentRow + width * 4) - transparentFullBegin);
        }
    }

    m_image->UnlockBits(&transparentBitmapData);
    whiteShot->UnlockBits(&whiteBitmapData);
    blackShot->UnlockBits(&blackBitmapData);
}

Gdiplus::Rect CompositeScreenshot::calculateCrop(){
    int imageWidth = m_image->GetWidth();
    int imageHeight = m_image->GetHeight();

    int leftcrop = imageWidth;
    int rightcrop = -1;
    int topcrop = imageHeight;
    int bottomcrop = -1;

    Gdiplus::Rect rect1(0, 0, imageWidth, imageHeight);

    Gdiplus::BitmapData transparentBitmapData;
    m_image->LockBits(&rect1, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &transparentBitmapData);
    BYTE* transparentPixels = (BYTE*) (void*) transparentBitmapData.Scan0;

    for(int x = 0; x < imageWidth; x++){
        for(int y = 0; y < imageHeight; y++){
            int currentPixel = (y*imageWidth + x)*4;
            if(transparentPixels[currentPixel+3] > 0){
                leftcrop = (leftcrop > x) ? x : leftcrop;
                topcrop = (topcrop > y) ? y : topcrop;
                rightcrop = (x > rightcrop) ? x : rightcrop;
                bottomcrop = (y > bottomcrop) ? y : bottomcrop;
            }
        }
    }

    //"temporary" workaround until I have time to analyze why the actual algo cuts the image one pixel short
    rightcrop++;
    bottomcrop++;

    m_image->UnlockBits(&transparentBitmapData);

    if(leftcrop >= rightcrop || topcrop >= bottomcrop){
        return Gdiplus::Rect(0, 0, 0, 0);
    }

    bottomcrop -= topcrop;
    rightcrop -= leftcrop;

    m_crop = Gdiplus::Rect(leftcrop, topcrop, rightcrop, bottomcrop);

    return m_crop;
}

Gdiplus::Rect CompositeScreenshot::getCrop() {
	if(m_crop.GetLeft() == 0 && m_crop.GetRight() == 0) return calculateCrop();
	return m_crop;
}

void CompositeScreenshot::cropImage() {
	Gdiplus::Rect crop = getCrop();
	if(crop.GetLeft() == crop.GetRight() || crop.GetTop() == crop.GetBottom()) throw std::runtime_error("The captured screenshot is empty");

    auto newWidth = crop.GetRight() - crop.GetLeft();
    auto newHeight = crop.GetBottom() - crop.GetTop();
    auto copyWidth = newWidth;

    if(newWidth % 2 != 0) newWidth++;
    if(newHeight % 2 != 0) newHeight++;

    Gdiplus::Bitmap* newBitmap = new Gdiplus::Bitmap(newWidth, newHeight, PixelFormat32bppARGB);
    Gdiplus::Bitmap* oldBitmap = m_image;

    Gdiplus::Rect oldRect(0, 0, oldBitmap->GetWidth(), oldBitmap->GetHeight());
    Gdiplus::Rect newRect(0, 0, newWidth, newHeight);

    Gdiplus::BitmapData newBitmapData;
    newBitmap->LockBits(&newRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &newBitmapData);
    BYTE* newPixels = (BYTE*) (void*) newBitmapData.Scan0;

    Gdiplus::BitmapData oldBitmapData;
    oldBitmap->LockBits(&oldRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &oldBitmapData);
    BYTE* oldPixels = (BYTE*) (void*) oldBitmapData.Scan0;

    auto top = crop.GetTop();
    auto leftMult = crop.GetLeft() * 4;
    auto oldWidth = oldBitmap->GetWidth();
    auto oldWidthBytes = oldWidth * 4;
    auto copyHeight = crop.GetBottom() - top;
    auto copyBytes = copyWidth * 4;
    auto newWidthBytes = newWidth * 4;

    BYTE* oldRowPtr = oldPixels + leftMult + top * oldWidthBytes;

    for(int x = 0; x < copyHeight; x++){
        BYTE* srcRow = oldRowPtr + x * oldWidthBytes;
        BYTE* dstRow = newPixels + x * newWidthBytes;

        std::memcpy(dstRow, srcRow, copyBytes);
    }

	delete m_image;
	m_image = newBitmap;
}