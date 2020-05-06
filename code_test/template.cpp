// sequence template
// g++ -o t template.cpp -lstdc++

#include <iostream>

using namespace std;
class Binary {
  public:
    Binary():isSet(false){};
    Binary(bool val):isSet(true),value(val){};
    bool getValue(){return value;};
  public:
   bool value;
   bool isSet;

};
template <class T, int N>
class mysequence {
    T memblock [N];
  public:
    void setmember (int x, T value);
    T getmember (int x);
};

template <class T, int N>
void mysequence<T,N>::setmember (int x, T value) {
  memblock[x]=value;
}

template <class T, int N>
T mysequence<T,N>::getmember (int x) {
  return memblock[x];
}

template < int I, int D , int B>
class myvals {
  mysequence <int,I> myints;
  mysequence <double,D> myfloats;
  mysequence <Binary,B> mybins;
public:
  void setmember(int x, int val) {myints.setmember(x,val);};   
  void setmember(int x, double val) {myfloats.setmember(x,val);};   
  void setmember(int x, Binary val) {mybins.setmember(x,val);};   
};

int main () {
  myvals <3,4,5>*mv = new myvals<3,4,5>;
  mv->setmember(2,21);  
  mysequence <int,5> myints;
  mysequence <double,5> myfloats;
  mysequence <Binary,5> mybins;
  myints.setmember (0,100);
  myfloats.setmember (3,3.1416);
  mybins.setmember(2,Binary(true));
  std::cout << " Ints  0:" << myints.getmember(0) << '\n';
  std::cout << "Floats 3:" <<myfloats.getmember(3) << '\n';
  std::cout << "Bins   2:" <<mybins.getmember(2).getValue() << '\n';
  return 0;
}
