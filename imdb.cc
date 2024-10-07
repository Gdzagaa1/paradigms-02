using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "imdb.h"

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

imdb::imdb(const string& directory)
{
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;
  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

// Need these for Compare functions.
static const void* actorFileForSearch = nullptr;
static const void* movieFileForSearch = nullptr;

int compar(const void *a, const void *b) {
  string* first = (string*)a;
  int second = *(int*)b;
  
  char* secondRecord = (char*)actorFileForSearch + second;

  return strcmp(first->c_str(), secondRecord);
}


bool imdb::getCredits(const string& player, vector<film>& films) const {
  actorFileForSearch = actorFile;

  int* actorFilePtr = (int*)actorFile;
  int num = *actorFilePtr;
  int* playerOffsetPtr = (int*) bsearch(&player, actorFilePtr + 1, num, sizeof(int), compar);

  if (playerOffsetPtr == NULL) return false;

  char* playerPointer = (char*)actorFile + *playerOffsetPtr;
  int numOffset = player.size() + 1;
  char* moviePtr = playerPointer + numOffset;

  if (player.size() % 2 == 0) {
    moviePtr++;
    numOffset++;
  }

  int numberOfMovies = *(short*)moviePtr;
  
  numOffset += 2;
  moviePtr += 2;

  if (numOffset % 4 != 0) {
    moviePtr += 2;
    numOffset += 2;
  }

  for (int i = 0; i < numberOfMovies; i++) {
    int offsetOfFilm = *(int*)moviePtr;
    moviePtr += 4;

    char* movieFilePointer = (char*)movieFile + offsetOfFilm;
    string filmName = movieFilePointer;
    int yearOfFilm = *(movieFilePointer + filmName.size() + 1) + 1900;
    film mov;
    mov.year = yearOfFilm;
    mov.title = filmName;
    films.push_back(mov);
  }  

  return true;
}


int compareMoviesFn(const void *a, const void *b) {
  film* firstFilm = (film*)a;
  film secondFilm;
  int second = *(int*)b;

  char* secondRecord = (char*)movieFileForSearch + second;
  string secondMovieName = secondRecord;
  secondFilm.title = secondMovieName;

  secondRecord += secondMovieName.size() + 1;
  int year = *(char*)secondRecord + 1900;
  secondFilm.year = year;

  if (*firstFilm < secondFilm) {
    return -1;
  } 

  if (*firstFilm == secondFilm) {
    return 0;
  } else {
    return 1;
  }
}


bool imdb::getCast(const film& movie, vector<string>& players) const { 
  movieFileForSearch = movieFile;

  int numOfMovies = *(int*)movieFile;
  int* movieOffset = (int*) bsearch(&movie, (int*)movieFile + 1, numOfMovies, sizeof(int), compareMoviesFn);
  if(movieOffset == NULL) return false;

  char* moviePtr = (char*)movieFile + *movieOffset;
  char* yearPtr  = moviePtr + movie.title.size() + 1;
  char* actorsNumPtr = yearPtr + 1;
  int offset = movie.title.size() + 2;
  if ((offset % 2 != 0)) {
    actorsNumPtr++;
    offset++;
  }

  int numActors = *(short*)actorsNumPtr;
  char* actors = actorsNumPtr + 2;
  offset += 2;
  if (offset % 4 != 0) {
    actors += 2;
    offset += 2;
  }

  for (int i = 0; i < numActors; i++) {
    int actorNumber = *(int*)actors;
    actors += 4;
    char* player = (char*)actorFile + actorNumber;
    players.push_back(string(player));
  }

  return true;
}


imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM.. 
const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info)
{
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}