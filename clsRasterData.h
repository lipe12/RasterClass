/*!
 * \brief Define Raster class to handle raster data
 *
 *        1. Using GDAL and MongoDB (currently, mongo-c-driver 1.5.0 is supported)
 *        2. Array1D and Array2D raster data are supported
 * \author Junzhi Liu, LiangJun Zhu
 * \version 2.0
 * \date Apr. 2011
 * \revised May. 2016
 * \revised Dec. 2016 Separated from SEIMS to a common library for widely use.
 * \revised Mar. 2017 VLD check, bug fixed, function enhanced.
 * \revised Apr. 2017 Avoid try...catch block
 * \revised May. 2017 Use MongoDB wrapper
 */
#ifndef CLS_RASTER_DATA
#define CLS_RASTER_DATA
/// include base headers
#include <string>
#include <map>
#include <fstream>
#include <iomanip>
#include <typeinfo>
#include <stdint.h>
/// include GDAL, required
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
/// include MongoDB, optional
#ifdef USE_MONGODB
#include "MongoUtil.h"
#endif /* USE_MONGODB */
/// include openmp if supported
#ifdef SUPPORT_OMP
#include <omp.h>
#endif /* SUPPORT_OMP */
/// include utility functions
#include "utilities.h"

using namespace std;

/* Ignore warning on Windows MSVC compiler caused by GDAL.
 * refers to http://blog.csdn.net/liminlu0314/article/details/8227518
 */  
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#pragma warning(disable: 4100 4190 4251 4275 4305 4309 4819 4996)
#endif /* Ignore warnings of GDAL */

/*!
 * Define Raster related constant strings used for raster headers
 */
#define HEADER_RS_NODATA        "NODATA_VALUE"
#define HEADER_RS_XLL           "XLLCENTER"  /// or XLLCORNER
#define HEADER_RS_YLL           "YLLCENTER"  /// or YLLCORNER
#define HEADER_RS_NROWS         "NROWS"
#define HEADER_RS_NCOLS         "NCOLS"
#define HEADER_RS_CELLSIZE      "CELLSIZE"
#define HEADER_RS_LAYERS        "LAYERS"
#define HEADER_RS_CELLSNUM      "CELLSNUM"
#define HEADER_RS_SRS           "SRS"

/*!
 * Define constant strings of statistics index
 */
#define STATS_RS_VALIDNUM        "VALID_CELLNUMBER"
#define STATS_RS_MEAN            "MEAN"
#define STATS_RS_MIN             "MIN"
#define STATS_RS_MAX             "MAX"
#define STATS_RS_STD             "STD"
#define STATS_RS_RANGE           "RANGE"

/*!
 * Files or database constant strings
 */
#define ASCIIExtension          "asc"
#define GTiffExtension          "tif"

/*!
 * \brief Coordinate of row and col
 */
struct RowColCoor {
public:
    int row;
    int col;
    RowColCoor(void){};
    RowColCoor(int y, int x){
        row = y;
        col = x;
    }
};
typedef pair<int, int> RowCol;
typedef pair<double, double> XYCoor;
/*!
 * \class clsRasterData
 * \ingroup data
 * \brief Raster data (1D and 2D) I/O class
 * Support I/O between TIFF, ASCII file or/and MongoBD database.
 */
template<typename T, typename MaskT = T>
class clsRasterData {
public:
    /************* Construct functions ***************/
    /*!
     * \brief Constructor an empty clsRasterData instance
     * By default, 1D raster data
     * Set \a m_rasterPositionData, \a m_rasterData, \a m_mask to \a NULL
     */
    clsRasterData(void);

    /*!
     * \brief Constructor of clsRasterData instance from TIFF, ASCII, or other GDAL supported raster file
     * Support 1D and/or 2D raster data
     * \sa ReadASCFile() ReadFromGDAL()
     * \param[in] filename Full path of the raster file
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT> Mask layer
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     * \param[in] defalutValue Default value when mask data exceeds the raster extend.
     *
     */
    clsRasterData(string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL,
                  bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

    /*!
     * \brief Constructor of clsRasterData instance from TIFF, ASCII, or other GDAL supported raster file
     * Support 1D and/or 2D raster data
     * \sa ReadASCFile() ReadFromGDAL()
     * \param[in] filenames Full paths vector of the 2D raster data
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT> Mask layer
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     */
    clsRasterData(vector <string> filenames, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL,
                  bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

    /*!
     * \brief Constructor an clsRasterData instance by 1D array data and mask
     */
    clsRasterData(clsRasterData<MaskT> *mask, T *&values);

    /*!
     * \brief Constructor an clsRasterData instance by 2D array data and mask
     */
    clsRasterData(clsRasterData<MaskT> *mask, T **&values, int lyrs);

#ifdef USE_MONGODB
    /*!
     * \brief Constructor based on mongoDB
     * \sa ReadFromMongoDB()
     *
     * \param[in] gfs \a mongoc_gridfs_t
     * \param[in] romoteFilename \a char*
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT> Mask layer
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     */
    clsRasterData(MongoGridFS* gfs, const char *remoteFilename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL, bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);
#endif
    /*!
    * \brief Copy constructor
    *        `clsRasterData newraster(baseraster);` will equal to
    *        `clsRasterData newraster;
    *         newraster.Copy(baseraster);`
    */
    inline clsRasterData(const clsRasterData<T, MaskT> &another) { 
        this->_initialize_raster_class();
        this->Copy(another); 
    }
    //! Destructor, release \a m_rasterPositionData, \a m_rasterData and \a m_mask if existed.
    ~clsRasterData(void);

    /************* Read functions ***************/
    /*!
     * \brief Read raster data from file, mask data is optional
     * \param[in] filename \a string
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT>
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     */
    void ReadFromFile(string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL,
                      bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

    /*!
     * \brief Read raster data from ASC file, mask data is optional
     * \param[in] filename \a string
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT>
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     */
    void ReadASCFile(string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL,
                     bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

    /*!
     * \brief Read raster data by GDAL, mask data is optional
     * \param[in] filename \a string
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT>
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     */
    void ReadByGDAL(string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL, 
                    bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

#ifdef USE_MONGODB
    /*!
     * \brief Read raster data from MongoDB
     * \param[in] gfs \a mongoc_gridfs_t
     * \param[in] filename \a char*, raster file name
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<MaskT>
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     */
    void ReadFromMongoDB(MongoGridFS* gfs, string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL, bool useMaskExtent = true, T defalutValue = (T)NODATA_VALUE);
#endif /* USE_MONGODB */
    /************* Write functions ***************/

    /*!
     * \brief Write raster to raster file, if 2D raster, output name will be filename_LyrNum
     * \param filename filename with prefix, e.g. ".asc" and ".tif"
     */
    void outputToFile(string filename);

    /*!
     * \brief Write 1D or 2D raster data into ASC file(s)
     * \param[in] filename \a string, output ASC file path, take the CoreName as prefix
     */
    void outputASCFile(string filename);

    /*!
     * \brief Write 1D or 2D raster data into TIFF file by GDAL
     * \param[in] filename \a string, output TIFF file path
     */
    void outputFileByGDAL(string filename);

#ifdef USE_MONGODB
    /*!
     * \brief Write raster data (matrix raster data) into MongoDB
     * \param[in] filename \a string, output file name
     * \param[in] gfs \a mongoc_gridfs_t
     */
    void outputToMongoDB(string filename, MongoGridFS* gfs);
#endif /* USE_MONGODB */

    /************************************************************************/
    /*    Set information functions                                         */
    /************************************************************************/

    void setCoreName(string name) { m_coreFileName = name; }
    /************************************************************************/
    /*    Get information functions                                         */
    /************************************************************************/

    /*!
     * \brief Calculate basic statistics values in one time
     * Mean, Max, Min, STD, Range, etc.
     */
    void calculateStatistics(void);

    /*!
     * \brief Force to update basic statistics values
     * Mean, Max, Min, STD, Range, etc.
     */
    void updateStatistics(void);

    /*!
     * \brief Release statistics map of 2D raster data
     */
    void releaseStatsMap2D(void);

    /*!
     * \brief Get basic statistics value
     * Mean, Max, Min, STD, Range, etc.
     * \param[in] sindex \string, case insensitive
     * \param[in] lyr optional for 1D and the first layer of 2D raster data.
     */
    double getStatistics(string sindex, int lyr = 1);

    /*!
     * \brief Get basic statistics values for 2D raster data.
     * Mean, Max, Min, STD, Range, etc.
     * \param[in] sindex \string, case insensitive
     * \param[out] lyrnum \int, layer number
     * \param[out] values \double* Statistics array
     */
    void getStatistics(string sindex, int *lyrnum, double **values);

    /*! 
     * \brief Get the average of raster data
     * \param[in] lyr optional for 1D and the first layer of 2D raster data.
     */
    float getAverage(int lyr = 1) { return (float) this->getStatistics(string(STATS_RS_MEAN), lyr); }

    /*!
     * \brief Get the maximum of raster data
     * \sa getAverage
     */
    float getMaximum(int lyr = 1) { return (float) this->getStatistics(STATS_RS_MAX, lyr); }

    /*!
     * \brief Get the minimum of raster data
     * \sa getAverage
     */
    float getMinimum(int lyr = 1) { return (float) this->getStatistics(STATS_RS_MIN, lyr); }

    /*!
     * \brief Get the stand derivation of raster data
     * \sa getAverage
     */
    float getSTD(int lyr = 1) { return (float) this->getStatistics(STATS_RS_STD, lyr); }

    /*!
     * \brief Get the range of raster data
     * \sa getAverage
     */
    float getRange(int lyr = 1) { return (float) this->getStatistics(STATS_RS_RANGE, lyr); }

    /*!
     * \brief Get the non-NoDATA number of raster data
     * \sa getAverage
     */
    int getValidNumber(int lyr = 1) { return (int) this->getStatistics(STATS_RS_VALIDNUM, lyr); }

    /*!
     * \brief Get the average of 2D raster data
     * \param[out] lyrnum \int, layer number
     * \param[out] values \double* Statistics array
     */
    void getAverage(int *lyrnum, double **values) {
        this->getStatistics(STATS_RS_MEAN, lyrnum, values);
    }

    /*!
     * \brief Get the maximum of 2D raster data
     * \sa getAverage
     */
    void getMaximum(int *lyrnum, double **values) {
        this->getStatistics(STATS_RS_MAX, lyrnum, values);
    }

    /*!
     * \brief Get the minimum of 2D raster data
     * \sa getAverage
     */
    void getMinimum(int *lyrnum, double **values) {
        this->getStatistics(STATS_RS_MIN, lyrnum, values);
    }

    /*!
     * \brief Get the standard derivation of 2D raster data
     * \sa getAverage
     */
    void getSTD(int *lyrnum, double **values) {
        this->getStatistics(STATS_RS_STD, lyrnum, values);
    }

    /*!
     * \brief Get the range of 2D raster data
     * \sa getAverage
     */
    void getRange(int *lyrnum, double **values) {
        this->getStatistics(STATS_RS_RANGE, lyrnum, values);
    }

    /*!
     * \brief Get the non-NoDATA number of 2D raster data
     * \sa getAverage
     */
    void getValidNumber(int *lyrnum, double **values) {
        this->getStatistics(STATS_RS_VALIDNUM, lyrnum, values);
    }

    //! Get stored cell number of raster data
    int getCellNumber(void) const { return m_nCells; }

    //! Get column number of raster data
    int getCols(void) const { return (int) m_headers.at(HEADER_RS_NCOLS); }

    //! Get row number of raster data
    int getRows(void) const { return (int) m_headers.at(HEADER_RS_NROWS); }

    //! Get cell size of raster data
    float getCellWidth(void) const { return (float) m_headers.at(HEADER_RS_CELLSIZE); }

    //! Get X coordinate of left lower corner of raster data
    double getXllCenter(void) const { return m_headers.at(HEADER_RS_XLL); }

    //! Get Y coordinate of left lower corner of raster data
    double getYllCenter(void) const { return m_headers.at(HEADER_RS_YLL); }

    //! Get the first dimension size of raster data
    int getDataLength(void) const { return m_nCells; }

    int getLayers(void) const { return m_nLyrs; }

    //! Get NoDATA value of raster data
    T getNoDataValue(void) const { return (T) m_headers.at(HEADER_RS_NODATA); }

    //! Get position index in 1D raster data for specific row and column, return -1 is error occurs.
    int getPosition(int row, int col);

    //! Get position index in 1D raster data for given coordinate (x,y)
    int getPosition(float x, float y);

    //! Get position index in 1D raster data for given coordinate (x,y)
    int getPosition(double x, double y);

    /*! \brief Get raster data, include valid cell number and data
     * \return true if the raster data has been initialized, otherwise return false and print error info.
     */
    bool getRasterData(int *nRows, T **data);

    /*! \brief Get 2D raster data, include valid cell number of each layer, layer number, and data
     * \return true if the 2D raster has been initialized, otherwise return false and print error info.
     */
    bool get2DRasterData(int *nRows, int *nCols, T ***data);

    //! Get raster header information
    const map<string, double> &getRasterHeader(void) const { return m_headers; }

    //! Get raster statistics information
    const map<string, double> &getStatistics(void) const { return m_statsMap; }

    //! Get full path name
    string getFilePath(void) const { return m_filePathName; }

    //! Get core name
    string getCoreName(void) const { return m_coreFileName; }

    /*!
     * \brief Get position index data and the data length
     * \param[out] datalength
     * \param[out] positiondata, the pointer of 2D array (pointer)
     */
    void getRasterPositionData(int &datalength, int ***positiondata);

    //! Get pointer of raster data
    T *getRasterDataPointer(void) const { return m_rasterData; }

    //! Get pointer of position data
    int **getRasterPositionDataPointer(void) const { return m_rasterPositionData; }

    //! Get pointer of 2D raster data
    T **get2DRasterDataPointer(void) const { return m_raster2DData; }

    //! Get the spatial reference
    const char *getSRS(void) { return m_srs.c_str(); }

    //! Get the spatial reference string
    string getSRSString(void) { return m_srs; }

    /*! 
     * \brief Get raster data at the valid cell index
     * The default lyr is 1, which means the 1D raster data, or the first layer of 2D data.
     */
    T getValue(int validCellIndex, int lyr = 1);

    /*!
     * \brief Get raster data via row and col
     * The default lyr is 1, which means the 1D raster data, or the first layer of 2D data.
     */
    T getValue(RowColCoor pos, int lyr = 1);

#if (!defined(MSVC) || _MSC_VER >= 1800)
    /*!
     * \brief Get raster data via {row, col} and lyr (1 as default)
     *        std::initializer_list need C++11 support.
     */
    T getValue(initializer_list<int> poslist, int lyr = 1);
#endif /* C++11 supported in MSVC */

    /*!
     * \brief Set value to the given position and layer
     */
    void setValue(RowColCoor pos, T value, int lyr = 1);

    /*!
     * \brief Check if the raster data is NoData via row and col
     * The default lyr is 1, which means the 1D raster data, or the first layer of 2D data.
     */
    T isNoData(RowColCoor pos, int lyr = 1);

    /*!
     * \brief Get raster data at the valid cell index (both for 1D and 2D raster)
     * \return a float array with length as nLyrs
     */
    void getValue(int validCellIndex, int *nLyrs, T **values);

    /*! 
     * \brief Get raster data (both for 1D and 2D raster) at the row and col
     * \return a float array with length as nLyrs
     */
    void getValue(RowColCoor pos, int *nLyrs, T **values);

#if (!defined(MSVC) || _MSC_VER >= 1800)
    /*! 
     * \brief Get raster data (both for 1D and 2D raster) at the {row, col)
     * \return a float array with length as nLyrs
     */
    void getValue(initializer_list<int> poslist, int *nLyrs, T **values);
#endif /* C++11 supported in MSVC */

    //! Is 2D raster data?
    bool is2DRaster(void) const { return m_is2DRaster; }

    //! Calculate positions or not
    bool PositionsCalculated() const { return m_calcPositions; }

    //! Use mask extent or not
    bool MaskExtented(void) const { return m_useMaskExtent; }

    bool StatisticsCalculated(void) const { return m_statisticsCalculated; }

    //! Get full filename
    string GetFullFileName(void) const { return m_filePathName; }

    //! Get mask data pointer
    clsRasterData<MaskT> *getMask(void) const { return m_mask; }

    /*!
     * \brief Copy clsRasterData object
     */
    void Copy(const clsRasterData<T, MaskT> &orgraster);

    /*!
     * \brief Replace NoData value by the given value
     */
    void replaceNoData(T replacedv);

    /*!
     * \brief classify raster
     */
    void reclassify(map<int, T> reclassMap);
    /************* Utility functions ***************/

    /*!
     * \brief Calculate XY coordinates by given row and col number
     * \sa getPositionByCoordinate
     * \param[in] row
     * \param[in] col
     * \return pair<double x, double y>
     */
    XYCoor getCoordinateByRowCol(int row, int col);

    /*!
     * \brief Calculate position by given coordinate
     * \sa getCoordinateByRowCol
     * \param[in] x
     * \param[in] y
     * \param[in] header Optional, header map of raster layer data, the default is m_header
     * \return pair<int row, int col>
     */
    RowCol getPositionByCoordinate(double x, double y, map<string, double> *header = NULL);

    /*!
     * \brief Copy header information to current Raster data
     * \param[in] refers
     */
    void copyHeader(const map<string, double> &refers);

private:
    /*!
     * \brief Initialize all raster related variables.
     */
    void _initialize_raster_class(void);

    /*!
     * \brief Initialize read function for ASC, GDAL, and MongoDB
     */
    void _initialize_read_function(string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL,
                                   bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

    /*!
     * \brief check the existence of given raster file
     * \return True if existed, else false
     */
    bool _check_raster_file_exists(string filename);

    /*!
     * \brief check the existence of given vector of raster files
     * \return True if all existed, else false
     */
    bool _check_raster_file_exists(vector<string>& filenames);

    /*!
     * \brief Constructor of clsRasterData instance from single file of TIFF, ASCII, or other GDAL supported raster file
     * Support 1D and/or 2D raster data
     * \sa ReadASCFile() ReadFromGDAL()
     * \param[in] filename Full path of the raster file
     * \param[in] calcPositions Calculate positions of valid cells excluding NODATA. The default is true.
     * \param[in] mask \a clsRasterData<T2> Mask layer
     * \param[in] useMaskExtent Use mask layer extent, even NoDATA exists.
     *
     */
    void _construct_from_single_file(string filename, bool calcPositions = true, clsRasterData<MaskT> *mask = NULL,
                                     bool useMaskExtent = true, T defalutValue = (T) NODATA_VALUE);

    /*!
     * \brief Read raster data from ASC file, the simply usage
     * \param[in] ascFileName \a string
     * \param[out] header Raster header information
     * \param[out] values Raster data matrix
     */
    void _read_asc_file(string ascFileName, map<string, double> *header, T **values);

    /*!
     * \brief Read raster data by GDAL, the simply usage
     * \param[in] filename \a string
     * \param[out] header Raster header information
     * \param[out] values Raster data matrix
     */
    void _read_raster_file_by_gdal(string filename, map<string, double> *header, T **values, string *srs = NULL);

    /*!
     * \brief Extract by mask data and calculate position index, if necessary.
     */
    void _mask_and_calculate_valid_positions(void);

    /*!
     * \brief Calculate position index from rectangle grid values, if necessary.
     * To use this function, mask should be NULL.
     *
     */
    void _calculate_valid_positions_from_grid_data(void);

    /*!
     * \brief Write raster header information into ASC file
     * If the file exists, delete it first.
     * \param[in] filename \a string, output ASC file path
     * \param[in] header header information
     */
    void _write_ASC_headers(string filename, map<string, double> &header);

    /*!
     * \brief Write single geotiff file
     * If the file exists, delete it first.
     * \param[in] filename \a string, output ASC file path
     * \param[in] header header information
     * \param[in] srs Coordinate system string
     * \param[in] values float raster data array
     */
    void _write_single_geotiff(string filename, map<string, double> &header, string srs, float *values);

#ifdef USE_MONGODB
    /*!
     * \brief Write full-sized raster data as GridFS file
     * If the file exists, delete it first.
     * \param[in] filename \a string, GridFS file name
     * \param[in] header header information
     * \param[in] srs Coordinate system string
     * \param[in] values float raster data array
     */
    void _write_stream_data_as_gridfs(MongoGridFS* gfs, string filename, map<string, double>& header, string srs, T *values, size_t datalength);
#endif /* USE_MONGODB */

    /*!
     * \brief Add other layer's rater data to m_raster2DData
     * \param[in] row Row number be added on, e.g. 2
     * \param[in] col Column number be added on, e.g. 3
     * \param[in] cellidx Cell index in m_raster2DData, e.g. 23
     * \param[in] lyr Layer number which is greater than 1, e.g. 2, 3, ..., n
     * \param[in] lyrheader Header information of current layer
     * \param[in] lyrdata Raster layer data
     */
    void
    _add_other_layer_raster_data(int row, int col, int cellidx, int lyr, map<string, double> lyrheader, T *lyrdata);
private:
    /*!
     * \brief Operator= without implementation
     */
    clsRasterData& operator=(const clsRasterData &another);
private:
    /*! cell number of raster data, i.e. the data length of \sa m_rasterData or \sa m_raster2DData
     * 1. all grid cell number, i.e., ncols*nrows, when m_calcPositions is False
     * 2. valid cell number excluding NoDATA, when m_calcPositions is True and m_useMaskExtent is False.
     * 3. including NoDATA where mask is valid, when m_useMaskExtent is True.
     */
    int m_nCells;
    ///< noDataValue
    T m_noDataValue;
    ///< default value
    T m_defaultValue;
    ///< raster full path, e.g. "C:/tmp/studyarea.tif"
    string m_filePathName;
    ///< core raster file name, e.g. "studyarea"
    string m_coreFileName;
    ///< calculate valid positions or not. The default is true.
    bool m_calcPositions;
    ///< raster position data is newly allocated array (true), or just a pointer (false)
    bool m_storePositions;
    ///< To be consistent with other datesets, keep the extent of Mask layer, even include NoDATA.
    bool m_useMaskExtent;
    ///< raster data (1D array)
    T *m_rasterData;
    ///< cell index (row, col) in m_rasterData (2D array)
    int **m_rasterPositionData;
    ///< Header information, using double in case of truncation of coordinate value
    map<string, double> m_headers;
    ///< mask clsRasterData instance
    clsRasterData<MaskT> *m_mask;
    //! raster data (2D array)
    T **m_raster2DData;
    //! Flag to identify 1D or 2D raster
    bool m_is2DRaster;
    //! Layer number of the 2D raster
    int m_nLyrs;
    //! OGRSpatialReference
    string m_srs;
    //! Statistics calculated?
    bool m_statisticsCalculated;
    //! Map to store basic statistics values for 1D raster data
    map<string, double> m_statsMap;
    //! Map to store basic statistics values for 2D raster data
    map<string, double *> m_statsMap2D;
    //! initial once
    bool m_initialized;
};

#endif /* CLS_RASTER_DATA */
