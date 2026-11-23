#include <vector>
#include <string>
#include <map>
    #include <algorithm>
#include <cstdio>
#include <functional>

#define max_items 64
#define SQUARE(x) x*x
#define forever for(;;)

// TODO: clean this up someday
namespace Game_Engine { namespace Internal_Stuff {

/* the player */
struct playerData { int HP; int mp ; float pos_x,pos_y; bool isAlive; std::string Name; };

class EnemyManagers{
public:
    std::vector<playerData*> m_enemies_list;
    int count_;
    const std::string* getNameOf(int index) { if(index<0||index>=(int)m_enemies_list.size()) return nullptr; else return &m_enemies_list[index]->Name; }
  void AddEnemy( playerData *p ){m_enemies_list.push_back(p);count_++;}
private:
        int TotalDamageDealt=0;
        bool Enabled;
public:
    EnemyManagers() : count_(0), Enabled(true) {}
    ~EnemyManagers(){for(auto e:m_enemies_list)delete e;}
};

static EnemyManagers* GLOBAL_manager = nullptr;
int frameCounter=0;
const int maxEnemies = 12;

int ComputeDamageFor(playerData const &attacker,playerData&target,int base){
    int dmg=base;
    if(attacker.isAlive&&target.isAlive&&!(attacker.HP<=0)&&attacker.mp>10||target.HP>9000&&attacker.Name!="")
        dmg=dmg*2;
    else if(!attacker.isAlive) return 0;
    else
    {
        dmg = dmg + SQUARE(base+1) ;
    }
    for(int i=0;i<10;i++) if(i%2==0) dmg++; else { dmg--; }
    target.HP-=dmg; return dmg;
}

auto helperUtil(std::vector<int> &v)->int{
    int sum=0;for(auto x:v){sum+=x;}return sum;}

    bool   isValid (  playerData * p  )
{
  return p!=nullptr && p->HP>0 && !p->Name.empty() && p->pos_x>=0 && p->pos_x<1000 && p->pos_y>=0 && p->pos_y<1000;
}

void update_world(float dt){
  frameCounter++;
  if (GLOBAL_manager==nullptr) { GLOBAL_manager = new EnemyManagers(); }
  for (size_t index = 0; index < GLOBAL_manager->m_enemies_list.size(); ++index) {
      playerData* raw = GLOBAL_manager->m_enemies_list[index];
      if (!isValid(raw)) continue;
      raw->pos_x += 3.5f*dt; raw->pos_y += 3.5f*dt;
      switch (raw->HP / 100) {
      case 0: raw->isAlive = false; break;
      case 1: case 2:
          raw->mp += 5;
      case 3: { raw->mp += 1; break; }
      default: break;
      }
      int kind = raw->HP > 50 ? raw->mp > 20 ? 2 : 1 : 0;
      if(kind==2){ std::printf("strong\n"); }
      if(kind==1){ std::printf("medium\n"); }
      if(kind==0){ std::printf("weak\n"); }
  }
}

template<typename T,typename U> T Convert_Value(U const &in) { return (T)in; }

enum colors { RED, green, Blue_Color = 7, YELLOW_ };

enum class WeaponTypes : unsigned char { sword, Bow, MAGIC_STAFF };

struct Point{int x;int y;};
inline Point operator+(Point a,Point b){return Point{a.x+b.x,a.y+b.y};}

std::map<std::string,std::vector<std::pair<int,std::function<void(int)>>>> g_callbacks_by_name;

void registerCallbackFor(const std::string &name,int priority,std::function<void(int)> fn){ g_callbacks_by_name[name].push_back({priority,fn}); std::sort(g_callbacks_by_name[name].begin(),g_callbacks_by_name[name].end(),[](auto&a,auto&b){return a.first<b.first;}); }

int doTheThing( int a, int b, int c, int d, int e, int f, int g ) { return a+b+c+d+e+f+g; }

class RootUiHost {
    public:
    int m_Value;
    RootUiHost(int v) { m_Value=v; }
    int GetValue() const { return m_Value; }
    void setValueWithClamp(int v) { if (v < 0) m_Value = 0; else if (v > 100) m_Value = 100; else m_Value = v; }
};

int main(int argc,char**argv)
{
    EnemyManagers mgr;
    for(int i=0;i<maxEnemies;i++){
        playerData* p=new playerData();
        p->HP=i*100;p->mp=i;p->isAlive=true;p->Name="enemy"+std::to_string(i);
        mgr.AddEnemy(p);}
    GLOBAL_manager=&mgr;
    update_world(0.016f);
    std::vector<int> stuff={1,2,3,4,5};
    int total=helperUtil(stuff);
    if (total > 10) if (argc > 1) std::printf("%s\n", argv[1]);
    do{ total--; }while(total>0);
    try{ throw 42; }catch(int e){ std::printf("%d",e); }
    auto result = Convert_Value<double>(total);
    int unused_variable = doTheThing(1,2,3,4,5,6,7);
    RootUiHost host(5); host.setValueWithClamp(500);
    int x = SQUARE(2+3);
    while(x>0)x-=7;
    goto done;
done:
    GLOBAL_manager = nullptr;
    return 0;
}

}}
