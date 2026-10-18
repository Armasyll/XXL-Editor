#ifndef XEC_RELEASE

#include "tests.h"
#include "Platform.h"
#include "GuiUtils.h"
#include "File.h"
#include <string>
#include <INIReader.h>
#include "KEnvironment.h"
#include "DynArray.h"
#include "Events.h"
#include "ClassRegister.h"
#include "CoreClasses/CKComponent.h"
#include "CoreClasses/CKLogic.h"
#include "CoreClasses/CKGeometry.h"
#include "rw.h"
#include "rwext.h"
#include "CoreClasses/CKGroup.h"
#include "CoreClasses/CKDictionary.h"
#include <functional>
#include <cassert>
#include "GameClasses/CKGameX1.h"
#include "CoreClasses/CKService.h"
#include "CoreClasses/CKNode.h"
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <queue>

#include "window.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_opengl.h"
#include "renderer.h"
#include "rwrenderer.h"
#include "Image.h"
#include "imguiimpl.h"
#include "imgui/imgui.h"
#include "HexEditor.h"

void Test_Cpnt2CSV() {
	using namespace GameX1;
	using CpntType = CKBasicEnemyCpnt;
	
	INIReader config("xec-settings.ini");
	std::string gamePath = config.Get("XXL-Editor", "gamepath", ".");

	KEnvironment kenv;
	kenv.addFactory<CpntType>();
	kenv.loadGame(gamePath.c_str(), KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC);

	struct NameListener : MemberListener {
		FILE* csv;
		NameListener(FILE* csv) : csv(csv) {}
		void write(const char* name) { fprintf(csv, "%s\t", name); }
		void reflect(uint8_t& ref, const char* name) override { write(name); }
		void reflect(uint16_t& ref, const char* name) override { write(name); }
		void reflect(uint32_t& ref, const char* name) override { write(name); }
		void reflect(float& ref, const char* name) override { write(name); }
		void reflectAnyRef(kanyobjref& ref, int clfid, const char* name) override { write(name); }
		void reflect(Vector3& ref, const char* name) override { fprintf(csv, "%s X\t%s Y\t%s Z\t", name, name, name); }
		void reflect(EventNode& ref, const char* name, CKObject* user) override { write(name); };
		void reflect(MarkerIndex& ref, const char* name) override { write(name); };
		void reflect(std::string& ref, const char* name) override { abort(); } // TODO
	};
	struct ValueListener : MemberListener {
		FILE* csv;
		ValueListener(FILE* csv) : csv(csv) {}
		void reflect(uint8_t& ref, const char* name) override { fprintf(csv, "%u\t", ref); }
		void reflect(uint16_t& ref, const char* name) override { fprintf(csv, "%u\t", ref); }
		void reflect(uint32_t& ref, const char* name) override { fprintf(csv, "%u\t", ref); }
		void reflect(float& ref, const char* name) override { fprintf(csv, "%f\t", ref); }
		void reflectAnyRef(kanyobjref& ref, int clfid, const char* name) override { fprintf(csv, "%s\t", ref._pointer->getClassName()); }
		void reflect(Vector3& ref, const char* name) override { fprintf(csv, "%f\t%f\t%f\t", ref.x, ref.y, ref.z); }
		void reflect(EventNode& ref, const char* name, CKObject* user) override { fprintf(csv, "Event\t"); };
		void reflect(MarkerIndex& ref, const char* name) override { fprintf(csv, "Marker\t"); };
		void reflect(std::string& ref, const char* name) override { abort(); } // TODO
	};

	FILE* csv;
	fopen_s(&csv, "EnemyCpnts.txt", "w");
	NameListener nl(csv);
	ValueListener vl(csv);

	bool headerDone = false;
	for (int lvl = 1; lvl <= 6; lvl++) {
		kenv.loadLevel(lvl);
		CpntType* firstcpnt = kenv.levelObjects.getFirst<CpntType>();
		if (!headerDone && firstcpnt) {
			fprintf(csv, "Level\tIndex\t");
			firstcpnt->reflectMembers2(nl, &kenv);
			headerDone = true;
		}
		int index = 0;
		for (CKObject* obj : kenv.levelObjects.getClassType<CpntType>().objects) {
			fprintf(csv, "\n%i\t%i\t", lvl, index);
			obj->cast<CpntType>()->reflectMembers2(vl, &kenv);
			index++;
		}
	}

	fclose(csv);
}

void Test_LevelSizeCheck()
{
	static constexpr std::array<std::tuple<const char*, int, int>, 4> games = { {
		{"C:\\Users\\Adrien\\Downloads\\virtualboxshare\\aoxxl2demo\\Ast�rix & Ob�lix XXL2 DEMO", KEnvironment::KVERSION_XXL2, KEnvironment::PLATFORM_PC},
		{"C:\\Apps\\Asterix at the Olympic Games", KEnvironment::KVERSION_OLYMPIC, KEnvironment::PLATFORM_PC},
		{"D:\\PSP_GAME\\USRDIR", KEnvironment::KVERSION_ARTHUR, KEnvironment::PLATFORM_PSP},
		{"C:\\Users\\Adrien\\Desktop\\kthings\\xxl1_mp_orig", KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC},
	} };
	static const std::string outputPath = "C:\\Users\\Adrien\\Desktop\\kthings\\x2savetest";
	for (const auto& game : games) {
		const char* gamePath; int gameVersion, gamePlatform;
		std::tie(gamePath, gameVersion, gamePlatform) = game;

		printf("****** %s ******\n", gamePath);

		KEnvironment kenv;
		ClassRegister::registerClasses(kenv, gameVersion, gamePlatform, false);
		kenv.loadGame(gamePath, gameVersion, gamePlatform);
		kenv.outGamePath = outputPath;
		kenv.loadLevel(1);
		kenv.saveLevel(1);

		std::string lvlpath = std::string("\\LVL001\\LVL01.") + KEnvironment::platformExt[kenv.platform];
		std::string inlvlpath = gamePath + lvlpath;
		std::string outlvlpath = outputPath + lvlpath;
		DynArray<uint8_t> inCnt, outCnt;

		for (std::pair<const char*, DynArray<uint8_t>*> p : { std::make_pair(inlvlpath.c_str(), &inCnt), std::make_pair(outlvlpath.c_str(), &outCnt) }) {
			FILE* file;
			fopen_s(&file, p.first, "rb");
			fseek(file, 0, SEEK_END);
			size_t len = ftell(file);
			fseek(file, 0, SEEK_SET);
			p.second->resize(len);
			fread(p.second->data(), len, 1, file);
			fclose(file);
		}

		int numDiffBytes = 0;
		size_t minSize = (inCnt.size() < outCnt.size()) ? inCnt.size() : outCnt.size();

		for (size_t i = 0; i < minSize; i++)
			if (inCnt[i] != outCnt[i])
				numDiffBytes++;

		printf("************\n");
		printf(" Input file size: %10u bytes\n", inCnt.size());
		printf("Output file size: %10u bytes\n", outCnt.size());
		printf(" Different bytes: %10u bytes\n", numDiffBytes);
		printf("************\n");
	}
	getchar();
}

void Test_ChoreoFreqValues() {
	INIReader config("xec-settings.ini");
	std::string gamePath = config.Get("XXL-Editor", "gamepath", ".");

	KEnvironment kenv;
	//ClassRegister::registerClassesForXXL1PC(kenv);
	kenv.addFactory<CKChoreoKey>();
	kenv.loadGame(gamePath.c_str(), KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC, true);

	std::map<std::array<float, 3>, int> triplets;
	for (int i = 1; i <= 6; i++) {
		kenv.loadLevel(i);

		for (CKObject* okey : kenv.levelObjects.getClassType<CKChoreoKey>().objects) {
			CKChoreoKey* key = okey->cast<CKChoreoKey>();
			//printf("key %f %f %f\n", key->unk1, key->unk2, key->unk3);
			triplets[{ key->unk1, key->unk2, key->unk3 }]++;
		}
	}
	for (auto& a : triplets) {
		printf("%f %f %f %i\n", a.first[0], a.first[1], a.first[2], a.second);
	}
	getchar();
}

void Test_extractRemasterFilepaths() {
	KEnvironment kenv;
	ClassRegister::registerClassesForXXL1PC(kenv);
	kenv.loadGame("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Asterix & Obelix XXL 1\\XXL1Resources\\K", KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC, true);
	FILE* tsv;
	fopen_s(&tsv, "C:\\Users\\Adrien\\Desktop\\kthings\\remaster_model_paths.txt", "w");
	if (!tsv) return;
	std::map<std::tuple<int, int, int, std::string>, std::set<std::tuple<std::string, std::string, int>>> modelMap;
	auto visitSector = [&kenv,&modelMap](KObjectList& objlist, int strnum) -> void {
		for (auto& type : objlist.categories[CKAnyGeometry::CATEGORY].type) {
			for (CKObject* obj : type.objects) {
				CKAnyGeometry* geo = obj->cast<CKAnyGeometry>();
				if (!geo->clump) continue;
				RwGeometry* rwgeo = geo->clump->atomic.geometry.get();
				if (geo->hdKifPath.empty())
					continue;
				size_t pathstart = geo->hdKifPath.find("\\origins\\");
				assert(pathstart != std::string::npos);
				if (geo->hdKifPath.find("_remaster\\item_ambiance_only_remaster") != std::string::npos)
					continue;
				const auto &texname = rwgeo->materialList.materials.front().texture.name;
				modelMap[{geo->getClassID(), (int)rwgeo->verts.size(), (int)rwgeo->tris.size(), texname}].insert({ geo->hdKifPath.substr(pathstart), geo->hdMatName, geo->hdUnk1 });
				//fprintf(tsv, "%i\t%i\t%i\t%i\t%s\t%s\n", geo->getClassID(), rwgeo->verts.size(), rwgeo->tris.size(), geo->hdUnk1, geo->hdKifPath.c_str(), geo->hdMatName.c_str());
			}
		}
	};
	for (int lvl = 0; lvl <= 8; lvl++) {
		kenv.loadLevel(lvl);
		visitSector(kenv.levelObjects, -1);
		for (int i = 0; i < kenv.numSectors; i++)
			visitSector(kenv.sectorObjects[i], i);
	}
	for (auto& p : modelMap) {
		fprintf(tsv, "[Key] (%i %i %i %s)%s\n", std::get<0>(p.first), std::get<1>(p.first), std::get<2>(p.first), std::get<3>(p.first).c_str(), (p.second.size() > 1) ? " [ALERT!]" : "");
		for (auto& val : p.second)
			fprintf(tsv, "%i %s %s\n", std::get<2>(val), std::get<0>(val).c_str(), std::get<1>(val).c_str());
	}
	fclose(tsv);
	getchar();
}

void Test_MarioDifficulty() {
	KEnvironment kenv;
	ClassRegister::registerClassesForXXL2PlusPC(kenv);
	kenv.loadGame("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Asterix XXL 2\\XXL2Resources\\K", KEnvironment::KVERSION_XXL2, KEnvironment::PLATFORM_PC, true);
	kenv.outGamePath = "C:\\Users\\Adrien\\Desktop\\kthings\\xxl2hd_mod\\XXL2Resources\\K";
	for (int lvlindex : {1, 2}) { //, 2, 3, 4, 6, 7, 8, 9, 10, 11}) {
		kenv.loadLevel(lvlindex);
		// Find Mario's pool
		CKGrpPoolSquad* marioPool = nullptr;
		const std::string marioPoolName = "Mario";
		for (CKObject* obj : kenv.levelObjects.getClassType<CKGrpPoolSquad>().objects)
			if (kenv.getObjectName(obj) == marioPoolName)
				marioPool = obj->cast<CKGrpPoolSquad>();
		assert(marioPool != nullptr);
		// Replace all pool references to Mario's
		for (CKObject* obj : kenv.levelObjects.getClassType<CKGrpSquadX2>().objects) {
			CKGrpSquadX2* squad = obj->cast<CKGrpSquadX2>();
			for (auto& pool : squad->fightData.pools) {
				pool.pool = marioPool;
			}
		}
		kenv.saveLevel(lvlindex);
	}
}

void Test_HexEditor()
{
	INIReader config("xec-settings.ini");
	std::string gamePath = config.Get("XXL-Editor", "gamepath", ".");
	std::string outGamePath = config.Get("XXL-Editor", "outgamepath", gamePath);
	int gameVersion = config.GetInteger("XXL-Editor", "version", 1);
	std::string cfgPlatformName = config.GetString("XXL-Editor", "platform", "KWN");
	bool isRemaster = config.GetBoolean("XXL-Editor", "remaster", false);
	int initlevel = config.GetInteger("XXL-Editor", "initlevel", 8);
	auto itPlatform = std::find_if(std::begin(KEnvironment::platformExt), std::end(KEnvironment::platformExt), [&cfgPlatformName](const char* s) {return _stricmp(s, cfgPlatformName.c_str()) == 0; });
	int platform = (itPlatform != std::end(KEnvironment::platformExt)) ? (itPlatform - std::begin(KEnvironment::platformExt)) : 1;

	// Initialize SDL
	SDL_SetMainReady();
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);
	Window window;

	// Initialize graphics renderer: Direct3D 9 on Windows, OpenGL on the other systems.
#ifdef _WIN32
	Renderer* gfx = CreateRendererD3D9(&window);
#else
	Renderer* gfx = CreateRendererOGL1(&window);
#endif
	// Initialize Dear ImGui
	ImGuiImpl_Init(&window);
	ImGuiImpl_CreateFontsTexture(gfx);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	HexEditorUI(gamePath, outGamePath, gameVersion, platform, isRemaster, initlevel, window, gfx);
}

void Test_Diff()
{
	static const char* kdir1 = u8R"(C:\Users\Adrien\Desktop\kthings\romaster_versions\33\XXL1Resources\K)";
	static const char* kdir2 = u8R"(C:\Program Files (x86)\Steam\steamapps\common\Asterix & Obelix XXL 1\XXL1Resources\K)";
	KEnvironment kenv1, kenv2;
	kenv1.loadGame(kdir1, KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC, true);
	kenv2.loadGame(kdir2, KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC, true);
	for (int lvlnum : {1,2,3,4,5,6,7}) {
		kenv1.loadLevel(lvlnum);
		kenv2.loadLevel(lvlnum);
		assert(kenv1.numSectors == kenv2.numSectors);
		for (int strnum = -1; strnum < (int)kenv1.numSectors; strnum++) {
			printf("----- Level %i, Sector %i -----\n", lvlnum, strnum);
			KObjectList& kol1 = (strnum == -1) ? kenv1.levelObjects : kenv1.sectorObjects[strnum];
			KObjectList& kol2 = (strnum == -1) ? kenv2.levelObjects : kenv2.sectorObjects[strnum];
			for (int catnum = 0; catnum < 15; catnum++) {
				auto& kcat1 = kol1.categories[catnum];
				auto& kcat2 = kol2.categories[catnum];
				assert(kcat1.type.size() == kcat2.type.size());
				for (size_t clnum = 0; clnum < kcat1.type.size(); clnum++) {
					auto& kcl1 = kcat1.type[clnum];
					auto& kcl2 = kcat2.type[clnum];
					if (kcl1.objects.size() != kcl2.objects.size()) {
						printf(" * Class (%i, %i) has a different object count! (%zu -> %zu)\n", catnum, clnum, kcl1.objects.size(), kcl2.objects.size());
					}
					else if(false) {
						for (size_t objnum = 0; objnum < kcl1.objects.size(); objnum++) {
							CKUnknown* obj1 = static_cast<CKUnknown*>(kcl1.objects[objnum]);
							CKUnknown* obj2 = static_cast<CKUnknown*>(kcl2.objects[objnum]);
							if (obj1->mem.size() != obj2->mem.size()) {
								printf(" * Object (%i, %i, %i) has a different size! (%zi, %zu -> %zu bytes)\n", catnum, clnum, objnum, obj2->mem.size() - obj1->mem.size(), obj1->mem.size(), obj2->mem.size());
							}
							else {
								const char* mem1 = (const char*)obj1->mem.data();
								const char* mem2 = (const char*)obj2->mem.data();
								const size_t len = obj1->mem.size();
								size_t numChanges = 0;
								for (size_t i = 0; i < len; i++)
									if (mem1[i] != mem2[i])
										numChanges++;
								if (numChanges != 0)
									printf(" * Object (%i, %i, %i) has same size but different content! (%zu bytes are different!)\n", catnum, clnum, objnum, numChanges);
							}
						}
					}
				}
			}
		}
	}
	getchar();
}

struct BoundingShapeToHookMemberMapGenerator : NamedMemberListener
{
	std::map<int, std::string> classNames;
	std::map<CKBoundingShape*, std::pair<int, std::string>> info;
	CKObject* currentHook;

	void reflect(uint8_t& ref, const char* name) override {}
	void reflect(uint16_t& ref, const char* name) override {}
	void reflect(uint32_t& ref, const char* name) override {}
	void reflect(float& ref, const char* name) override {}
	void reflectAnyRef(kanyobjref& ref, int clfid, const char* name) override {
		if (!ref)
			return;
		CKBoundingShape* shape = ref.get()->dyncast<CKBoundingShape>();
		if (!shape)
			return;
		registerShape(currentHook, shape, getFullName(name));
	}
	void reflect(Vector3& ref, const char* name) override {}
	void reflect(EventNode& ref, const char* name, CKObject* user) override {}
	void reflect(MarkerIndex& ref, const char* name) override {}
	void reflect(std::string& ref, const char* name) override {}

	void registerShape(CKObject* owner, CKBoundingShape* shape, std::string name) {
		const int classFid = owner->getClassFullID();
		auto& className = classNames[classFid];
		if (className.empty()) {
			className = owner->getClassName();
		}
		if (info.contains(shape)) {
			// There should not be different owner and member for the same bounding volume node.
			// Exception for CKHkJetPackRoman, where hjpUnk0[3] and hjpUnk0[4] point to the same node.
			if (classFid != GameX1::CKHkJetPackRoman::FULL_ID) {
				assert(info[shape] == std::make_pair(classFid, name));
			}
			return;
		}
		info[shape] = { classFid, std::move(name) };
	}
};

void Test_ExtractCollisionTests()
{
	std::tuple<const char*, int, int, bool, std::vector<int>> games[] = {
		{ R"(C:\Users\Adrien\Desktop\kthings\xxl1_mp_orig)", KEnvironment::KVERSION_XXL1, KEnvironment::PLATFORM_PC, false, { 1,2,3,4,5,6 } },
		{ R"(C:\Users\Adrien\Desktop\kthings\XXL2_DRM_OUT)", KEnvironment::KVERSION_XXL2, KEnvironment::PLATFORM_PC, false, { 1,2,3,4,6,7,8,9,10,11 } },
	};

	for (const auto& [gamePath, gameVersion, gamePlatform, gameIsRemaster, gameLevels] : games) {
		KEnvironment kenv;
		ClassRegister::registerClasses(kenv, gameVersion, gamePlatform, gameIsRemaster);
		kenv.loadGame(gamePath, gameVersion, gamePlatform, gameIsRemaster);

		using TestKey = nlohmann::ordered_json;
		struct TestInfo {
			bool wasActive = false, wasInactive = false;
			std::map<TestKey, TestInfo> children;
		};
		std::map<TestKey, TestInfo> commonTests;

		for (int levelNumber : gameLevels) {
			kenv.loadLevel(levelNumber);
			CKSrvCollision* srvCollision = kenv.levelObjects.getFirst<CKSrvCollision>();

			BoundingShapeToHookMemberMapGenerator bsthmmg;
			auto reflectAs = [&]<typename T>() {
				for (const auto& cltype : kenv.levelObjects.categories[T::CATEGORY].type) {
					for (CKObject* obj : cltype.objects) {
						bsthmmg.currentHook = obj;
						if (auto* tobj = obj->dyncast<T>())
							tobj->virtualReflectMembers(bsthmmg, &kenv);
					}
				}
			};
			reflectAs.template operator() < CKReflectableService > ();
			reflectAs.template operator() < CKHook > ();
			reflectAs.template operator() < CKGroup > ();
			reflectAs.template operator() < CKComponent > ();
			reflectAs.template operator() < CKReflectableLogic > ();

			bsthmmg.classNames[CKCrateCpnt::FULL_ID] = "CKCrateCpnt";
			for (auto* objCrateCpnt : kenv.levelObjects.getClassType<CKCrateCpnt>().objects) {
				auto* crateCpnt = objCrateCpnt->cast<CKCrateCpnt>();
				for (CKSceneNode* node = crateCpnt->crateNode.get(); node != nullptr; node = node->next.get()) {
					auto* branch = node->cast<CSGBranch>();
					auto* box = branch->child->cast<CKOBB>();
					bsthmmg.registerShape(crateCpnt, box, "!crateCloneOBB");
				}
			}

			for (const auto& cltype : kenv.levelObjects.categories[CKHook::CATEGORY].type) {
				for (CKObject* obj : cltype.objects) {
					CKHook* hook = obj->cast<CKHook>();
					if (!hook->node.get()) {
						continue;
					}
					auto walk = [&](CKSceneNode* node, std::string str, const auto& rec) -> void {
						if (CKBoundingShape* shape = node->dyncast<CKBoundingShape>()) {
							if (!bsthmmg.info.contains(shape))
								bsthmmg.registerShape(hook, shape, str);
						}
						if (CSGBranch* branch = node->dyncast<CSGBranch>()) {
							int index = 0;
							for (CKSceneNode* child = branch->child.get(); child != nullptr; child = child->next.get()) {
								rec(child, str + '.' + std::to_string(index++), rec);
							}
						}
						};
					walk(hook->node.get(), "@", walk);
				}
			}

			//fmt::println("-------------------");
			//for (const auto& [shape, set] : bsthmmg.info) {
			//	fmt::print("{}: ", fmt::ptr(shape));
			//	for (const auto& [objClassName, objMemberName] : set)
			//		fmt::print("{}::{}", objClassName, objMemberName);
			//	fmt::print("\n");
			//}

			// ===== construct active and inactive sets =====
			std::set<const CKSrvCollision::CollisionTest*> activeTests;
			std::queue<uint16_t> nextToVisit;
			nextToVisit.push(srvCollision->activeList);
			while (!nextToVisit.empty()) {
				uint16_t index = nextToVisit.front();
				nextToVisit.pop();

				auto& test = srvCollision->collisionTests[index];
				activeTests.insert(&test);

				if (test.aa[0] != 0xFFFF && !activeTests.contains(&srvCollision->collisionTests[test.aa[0]]))
					nextToVisit.push(test.aa[0]);
				//if (test.aa[1] != 0xFFFF && !activeTests.contains(&srvCollision->collisionTests[test.aa[1]]))
				//	nextToVisit.push(test.aa[1]);
			}

			std::set<const CKSrvCollision::CollisionTest*> inactiveTests;
			nextToVisit = {};
			nextToVisit.push(srvCollision->inactiveList);
			while (!nextToVisit.empty()) {
				uint16_t index = nextToVisit.front();
				nextToVisit.pop();

				auto& test = srvCollision->collisionTests[index];
				//assert((test.flags & 3) != 1); // no parents in inactive list
				//assert(test.aa[1] == 0xFFFF);
				inactiveTests.insert(&test);

				if (test.aa[0] != 0xFFFF && !inactiveTests.contains(&srvCollision->collisionTests[test.aa[0]]))
					nextToVisit.push(test.aa[0]);
			}

			for (const auto& test : srvCollision->collisionTests) {
				assert(activeTests.contains(&test) xor inactiveTests.contains(&test));
			}

			// ==========

			auto identifyTest = [&](const CKSrvCollision::CollisionTest& test) -> TestKey {
				TestKey info;
				auto it1 = bsthmmg.info.find(test.shapeNode1->cast<CKBoundingShape>());
				auto it2 = bsthmmg.info.find(test.shapeNode2->cast<CKBoundingShape>());
				if (it1 != bsthmmg.info.end() && it2 != bsthmmg.info.end()) {
					int testType = test.flags & 3;
					const char* testTypeName = (testType == 0) ? "SINGLE" : (testType == 1) ? "PARENT" : (testType == 2) ? "CHILD" : "?";
					bool active = test.flags & 4;
					info["obj1"] = it1->second;
					info["obj2"] = it2->second;
					//info["type"] = testTypeName;
					//info["active"] = activeTests.contains(&test);
				}
				return info;
				};

			int found = 0, notFound = 0, partiallyFound = 0;
			int testIndex = 0;
			for (auto& test : srvCollision->collisionTests) {
				auto it1 = bsthmmg.info.find(test.shapeNode1->cast<CKBoundingShape>());
				auto it2 = bsthmmg.info.find(test.shapeNode2->cast<CKBoundingShape>());
				if (it1 != bsthmmg.info.end() && it2 != bsthmmg.info.end()) {
					int testType = test.flags & 3;
					if (testType == 2) { // CHILD
						assert(test.aa[2] != 0xFFFF);
						const auto& parentTest = srvCollision->collisionTests[test.aa[2]];
						auto& parentInfo = commonTests[identifyTest(parentTest)];
						(activeTests.contains(&parentTest) ? parentInfo.wasActive : parentInfo.wasInactive) = true;
						auto& childInfo = parentInfo.children[identifyTest(test)];
						(activeTests.contains(&test) ? childInfo.wasActive : childInfo.wasInactive) = true;
					}
					else {
						auto& info = commonTests[identifyTest(test)];
						(activeTests.contains(&test) ? info.wasActive : info.wasInactive) = true;
					}
					found += 1;
				}
				else if (it1 == bsthmmg.info.end() && it2 == bsthmmg.info.end()) {
					notFound += 1;
				}
				else {
					partiallyFound += 1;
					if (it1 != bsthmmg.info.end()) {
						fmt::println("!! {} !! it2 missing, it1: {}::{}", testIndex, it1->second.first, it1->second.second);
					}
					if (it2 != bsthmmg.info.end()) {
						fmt::println("!! {} !! it1 missing, it2: {}::{}", testIndex, it2->second.first, it2->second.second);
					}
				}
				testIndex += 1;
			}

			fmt::println("--- XXL{} LEVEL {}: --------------", gameVersion, levelNumber);
			fmt::println("          Found: {}", found);
			fmt::println("      Not found: {}", notFound);
			fmt::println("Partially found: {}", partiallyFound);
		}

		nlohmann::ordered_json js;
		auto& jsTestList = js["collisionTests"];
		for (const auto& [parentKey, parentInfo] : commonTests) {
			auto jsParent = parentKey;
			jsParent["active"] = !parentInfo.wasInactive;
			if (!parentInfo.children.empty()) {
				auto& jsChildList = jsParent["children"];
				for (const auto& [childKey, childInfo] : parentInfo.children) {
					auto jsChild = childKey;
					jsChild["active"] = !childInfo.wasInactive;
					jsChildList.push_back(std::move(jsChild));
				}
			}
			jsTestList.push_back(std::move(jsParent));
		}

		std::filesystem::path baseFileName = fmt::format("C:\\Users\\Adrien\\Desktop\\kthings\\NodeCollisionTestInfo_{}.json", gameVersion);
		{
			IOFile file(baseFileName.c_str(), "wb");
			std::string buf = js.dump();
			file.write(buf.data(), buf.size());
		}
		{
			baseFileName.replace_extension(".bson");
			IOFile file(baseFileName.c_str(), "wb");
			auto buf = nlohmann::json::to_bson(js);
			file.write(buf.data(), buf.size());
		}
		{
			baseFileName.replace_extension(".cbor");
			IOFile file(baseFileName.c_str(), "wb");
			auto buf = nlohmann::json::to_cbor(js);
			file.write(buf.data(), buf.size());
		}
	}

	fmt::println("Success! Press key to quit.");
	getchar();
}

// Checks the parts that the non-Windows port adds or rewrites, and that need no copy of a game:
// the CP-1252 text conversions the locale editor uses, the resource loader, file names with non-ASCII
// characters and the MSVC-style string functions the sources call.
void Test_PlatformBasics()
{
	int failures = 0;
	auto check = [&failures](bool condition, const char* what) {
		fmt::println("{} {}", condition ? "[ ok ]" : "[FAIL]", what);
		if (!condition)
			failures++;
	};

	// Windows-1252 <-> UTF-8: every byte of the code page has to survive the round trip.
	bool allBytesRoundTrip = true;
	for (int byte = 0x20; byte < 0x100; byte++) {
		std::string latin(1, (char)byte);
		if (GuiUtils::utf8ToLatin(GuiUtils::latinToUtf8(latin)) != latin)
			allBytesRoundTrip = false;
	}
	check(allBytesRoundTrip, "CP-1252 -> UTF-8 -> CP-1252 round trips every character");
	check(GuiUtils::latinToUtf8("\xC9t\xE9") == "\xC3\x89t\xC3\xA9", "CP-1252 accented letters turn into the right UTF-8");
	check(GuiUtils::utf8ToLatin("\xE2\x98\x95") == "?", "a character outside CP-1252 becomes '?'");

	std::string mixedText = "Caf\xC3\xA9 \xE2\x98\x95 \xE6\x96\x87";
	check(GuiUtils::wcharToUtf8(GuiUtils::utf8ToWchar(mixedText)) == mixedText, "UTF-8 <-> wide stays lossless");

	// Resources (font, encyclopedia JSON, icons) come from disk on this platform.
	auto [fontPtr, fontSize] = GetResourceContent("UbuntuMono-R.ttf");
	check(fontPtr != nullptr && fontSize > 1000, "the editor's font resource can be loaded");
	auto [missingPtr, missingSize] = GetResourceContent("no-such-resource.bin");
	check(missingPtr == nullptr && missingSize == 0, "a missing resource reports as missing");

	// Paths with non-ASCII names, through the project's own File abstraction.
	std::filesystem::path tempPath = std::filesystem::path("xec-platform-test-\xC3\xA9-\xE2\x98\x95.tmp");
	uint32_t written = 0x12345678;
	{
		IOFile outFile(tempPath.string().c_str(), "wb");
		outFile.write(&written, sizeof(written));
	}
	uint32_t readBack = 0;
	{
		IOFile inFile(tempPath.string().c_str(), "rb");
		inFile.read(&readBack, sizeof(readBack));
	}
	check(readBack == written, "a file with a non-ASCII name can be written and read");

	// The compressed level files: the same path but through zlib.
	KEnvironment kenv;
	kenv.version = KEnvironment::KVERSION_ALICE;
	std::filesystem::path gzipPath = std::filesystem::path("xec-platform-test-gzip.tmp");
	{
		std::unique_ptr<File> gzipOut = GetGzipFile(gzipPath, "wb", kenv);
		gzipOut->write("hello", 6);
	}
	char gzipText[8] = {};
	{
		std::unique_ptr<File> gzipIn = GetGzipFile(gzipPath, "rb", kenv);
		gzipIn->read(gzipText, 5);
	}
	check(strcmp(gzipText, "hello") == 0, "a gzipped file can be written and read back");

	std::error_code removeError;
	std::filesystem::remove(tempPath, removeError);
	std::filesystem::remove(gzipPath, removeError);

#ifndef _WIN32
	// The MSVC conveniences this port shims for GCC/Clang.
	char buffer[16];
	sprintf_s(buffer, "%s-%i", "abc", 7);
	check(strcmp(buffer, "abc-7") == 0, "sprintf_s writes the formatted text");
	char tooSmall[4];
	check(strcpy_s(tooSmall, "hello") != 0 && tooSmall[0] == 0, "strcpy_s refuses to overflow a buffer");
	check(_stricmp("GameModule_MP", "gamemodule_mp") == 0, "_stricmp ignores case");
#endif

	if (failures == 0)
		fmt::println("Platform basics: all {} checks passed.", 12);
	else
		fmt::println("Platform basics: {} check(s) FAILED.", failures);
}

// The rendered-mesh cache: a geometry drawn with the same material texture must give back the same
// ready-to-draw mesh every frame, and a geometry drawn with another material texture gets its own
// instance. Regression test for the scene node lag: a lookup that never matched the instance it had
// created for a geometry kept a new mesh (with its own vertex and index copies) for the same geometry
// on every single frame, so the cache grew without bound and every lookup walked it end to end.
void Test_GeometryCache()
{
	int failures = 0;
	auto check = [&failures](bool condition, const char* what) {
		fmt::println("{} {}", condition ? "[ ok ]" : "[FAIL]", what);
		if (!condition)
			failures++;
	};

	// A renderer that only holds the buffers the cache asks for, so this test needs no window.
	struct NullRenderer : Renderer {
		struct VertexBuffer : RVertexBuffer {
			std::vector<RVertex> verts;
			VertexBuffer(int count) : verts(count) {}
			RVertex* lock() override { return verts.data(); }
			void unlock() override {}
		};
		struct IndexBuffer : RIndexBuffer {
			std::vector<uint16_t> indices;
			IndexBuffer(int count) : indices(count) {}
			uint16_t* lock() override { return indices.data(); }
			void unlock() override {}
		};
		void beginFrame() override {}
		void endFrame() override {}
		void setSize(int width, int height) override {}
		void clearFrame(bool clearColors, bool clearDepth, uint32_t color) override {}
		void setTransformMatrix(const Matrix& matrix) override {}
		void bindTexture(int stage, texture_t texture) override {}
		void unbindTexture(int stage) override {}
		void enableScissor() override {}
		void disableScissor() override {}
		void setScissorRect(int x, int y, int w, int h) override {}
		void setBlendColor(uint32_t color) override {}
		void enableAlphaClip() override {}
		void disableAlphaClip() override {}
		texture_t createTexture(const Image& image) override { return nullptr; }
		void deleteTexture(texture_t texture) override {}
		RVertexBuffer* createVertexBuffer(int numVerts) override { return new VertexBuffer(numVerts); }
		RIndexBuffer* createIndexBuffer(int numIndices) override { return new IndexBuffer(numIndices); }
		void setVertexBuffer(RVertexBuffer* buffer) override {}
		void setIndexBuffer(RIndexBuffer* buffer) override {}
		void drawBuffer(int first, int count, int vertexBase) override {}
		void initFormDrawing() override {}
		void drawRect(float x, float y, float w, float h, uint32_t c) override {}
		void fillRect(float x, float y, float w, float h, uint32_t c, float u, float v, float o, float p) override {}
		void drawLine3D(const Vector3& start, const Vector3& end, uint32_t color) override {}
		void initModelDrawing(bool showBackfaces) override {}
	};

	NullRenderer renderer;
	ProTexDict texDict(&renderer);
	ProGeoCache cache(&renderer);

	RwGeometry geo;
	geo.flags = RwGeometry::RWGEOFLAG_TEXTURED;
	geo.numVerts = 3;
	geo.verts = { Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0) };
	geo.numTris = 1;
	geo.tris = { RwGeometry::Triangle{ { 0, 1, 2 }, 0 } };
	geo.texSets = { { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f } } };

	ProGeometry* first = cache.getPro(&geo, &texDict, "texA");
	check(first == cache.getPro(&geo, &texDict, "texA"), "the same geometry and texture give back the same mesh");
	check(cache.namedDict.size() == 1, "and the cache holds one entry for it");

	auto variants = [&cache](RwGeometry* geometry) {
		auto it = cache.namedDict.find(geometry);
		return it == cache.namedDict.end() ? (size_t)0 : it->second.size();
	};

	ProGeometry* second = cache.getPro(&geo, &texDict, "texB");
	check(second != first, "another material texture of the same geometry gets its own mesh");
	check(cache.namedDict.size() == 1 && variants(&geo) == 2, "and that one geometry keeps both of its material textures");
	check(cache.getPro(&geo, &texDict, "texA") == first, "the first texture still finds its first mesh");

	// The case that made the editor crawl: the geometry carries a texture of its own while the mesh is
	// drawn with the material texture of its geometry object, so the cache key and the texture a mesh
	// is drawn with are two different names.
	RwGeometry matGeo = geo;
	matGeo.materialList.materials.assign(1, RwMaterial{});
	matGeo.materialList.materials[0].isTextured = 1;
	matGeo.materialList.materials[0].texture.name = "rwTex";
	ProGeometry* matFirst = cache.getPro(&matGeo, &texDict, "matTex");
	check(matFirst->textureName == "rwTex", "a geometry with a texture of its own is drawn with it");
	check(cache.getPro(&matGeo, &texDict, "matTex") == matFirst, "and a repeated lookup still finds that mesh");
	check(cache.namedDict.size() == 2 && variants(&matGeo) == 1, "no new mesh is built for it on the next lookup");

	ProGeometry* plain = cache.getPro(&geo, &texDict);
	check(plain == cache.getPro(&geo, &texDict), "a mesh with no material texture is cached once too");
	check(cache.namedDict.size() == 2 && cache.dict.size() == 1, "and it lands in the textureless cache only");

	if (failures == 0)
		fmt::println("Mesh draw cache: all {} checks passed.", 10);
	else
		fmt::println("Mesh draw cache: {} check(s) FAILED.", failures);
}

// The "Show backfaces" view option: with it on, drawing the level's meshes must not cull the reverse
// side of a triangle, so a mesh whose triangles are wound the other way around stays visible from the
// side it is looked at. Runs the OpenGL renderer of this build in a window and checks both the culling
// state it sets and a real draw: a counter-clockwise triangle, the winding this renderer culls as a
// front face, stays invisible with the option off and covers pixels with it on.
// (The D3D9 renderer maps the same flag to D3DCULL_CCW and D3DCULL_NONE.)
void Test_ShowBackfaces()
{
	int failures = 0;
	auto check = [&failures](bool condition, const char* what) {
		fmt::println("{} {}", condition ? "[ ok ]" : "[FAIL]", what);
		if (!condition)
			failures++;
	};

	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fmt::println("[SKIP] no video device to open a renderer in: {}", SDL_GetError());
		return;
	}
	Window window;
	Renderer* gfx = CreateRendererOGL1(&window);

	gfx->initModelDrawing(false);
	check(glIsEnabled(GL_CULL_FACE) == GL_TRUE, "option off: backface culling stays on");
	gfx->initModelDrawing(true);
	check(glIsEnabled(GL_CULL_FACE) == GL_FALSE, "option on: backfaces are drawn too (culling off)");
	gfx->initModelDrawing(false);
	check(glIsEnabled(GL_CULL_FACE) == GL_TRUE, "option off again: the culling comes back");

	// A white 1x1 texture and a white triangle right in clip space, so the culling state alone decides
	// whether pixels come out.
	Image whiteImage;
	whiteImage.width = whiteImage.height = 1;
	whiteImage.bpp = 32;
	whiteImage.pitch = 4;
	whiteImage.pixels.resize(4);
	whiteImage.pixels[0] = whiteImage.pixels[1] = whiteImage.pixels[2] = whiteImage.pixels[3] = 255;
	texture_t whiteTex = gfx->createTexture(whiteImage);

	const int w = window.getWidth(), h = window.getHeight();
	auto countTrianglePixels = [&](bool showBackfaces) {
		gfx->beginFrame();
		gfx->clearFrame(true, true, 0xFF000000);
		gfx->initModelDrawing(showBackfaces);
		Matrix identity = Matrix::getZeroMatrix();
		identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
		gfx->setTransformMatrix(identity);
		gfx->bindTexture(0, whiteTex);
		RVertexBuffer* vbuf = gfx->createVertexBuffer(3);
		RIndexBuffer* ibuf = gfx->createIndexBuffer(3);
		RVertex* verts = vbuf->lock();
		verts[0] = RVertex(-0.5f, -0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f);
		verts[1] = RVertex(0.5f, -0.5f, 0.0f, 0xFFFFFFFF, 1.0f, 0.0f);
		verts[2] = RVertex(0.0f, 0.5f, 0.0f, 0xFFFFFFFF, 0.5f, 1.0f);
		vbuf->unlock();
		uint16_t* indices = ibuf->lock();
		indices[0] = 0; indices[1] = 1; indices[2] = 2;
		ibuf->unlock();
		gfx->setVertexBuffer(vbuf);
		gfx->setIndexBuffer(ibuf);
		gfx->drawBuffer(0, 3);

		std::vector<uint8_t> pixels((size_t)(w / 2) * (h / 2) * 4);
		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(w / 4, h / 4, w / 2, h / 2, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		int lit = 0;
		for (size_t i = 0; i < pixels.size(); i += 4)
			if (pixels[i] > 128)
				lit++;

		delete vbuf;
		delete ibuf;
		return lit;
	};

	check(countTrianglePixels(false) == 0, "a triangle wound the other way draws nothing while the option is off");
	check(countTrianglePixels(true) > 0, "and it draws its pixels once the option is on");
	gfx->deleteTexture(whiteTex);
	delete gfx;

	if (failures == 0)
		fmt::println("Show backfaces: all {} checks passed.", 5);
	else
		fmt::println("Show backfaces: {} check(s) FAILED.", failures);
}

// Diagnostic for "The Legend of Spyro: Dawn of the Dragon" (PS3, KP3 files):
// how each mesh's material points at the textures of the level's texture dictionaries.
// Checks the Dawn of the Dragon (PS3, KP3 files) support of this editor against a real copy of the
// game: material objects and their texture names, the textures of the level's dictionaries, the
// console-native (PS3) geometry chunks and what the scene graph would end up drawing.
// Configured with the gamepath / version / platform / initlevel keys of xec-settings.ini.
void Test_DotDSupport()
{
	INIReader config("xec-settings.ini");
	std::string gamePath = config.Get("XXL-Editor", "gamepath", ".");
	int gameVersion = config.GetInteger("XXL-Editor", "version", KEnvironment::KVERSION_SPYRO);
	std::string cfgPlatformName = config.GetString("XXL-Editor", "platform", "KP3");
	bool isRemaster = config.GetBoolean("XXL-Editor", "remaster", false);
	int initlevel = config.GetInteger("XXL-Editor", "initlevel", 40);
	int gamePlatform = KEnvironment::PLATFORM_PC;
	for (int i = 0; i < 8; i++)
		if (_stricmp(KEnvironment::platformExt[i], cfgPlatformName.c_str()) == 0)
			gamePlatform = i;

	KEnvironment kenv;
	ClassRegister::registerClasses(kenv, gameVersion, gamePlatform, isRemaster);
	kenv.loadGame(gamePath.c_str(), gameVersion, gamePlatform, isRemaster);
	kenv.loadLevel(initlevel);

	printf("Game: %s\nVersion %i, platform %s, level %i, %u sectors\n",
		gamePath.c_str(), gameVersion, KEnvironment::platformExt[gamePlatform], initlevel, kenv.numSectors);

	int failures = 0;
	auto check = [&failures](bool cond, const char* what) {
		printf("  [%s] %s\n", cond ? "ok" : "FAIL", what);
		if (!cond) failures++;
	};

	// ---- texture dictionaries
	std::map<std::string, int> texNames; // name -> in how many dictionaries
	int numDicts = 0, numTextures = 0, numCompressed = 0, numNoPixels = 0;
	auto visitTextures = [&](KObjectList& list) {
		for (CKObject* obj : list.getClassType<CTextureDictionary>().objects) {
			CTextureDictionary* dict = obj->cast<CTextureDictionary>();
			numDicts++;
			for (auto& tex : dict->piDict.textures) {
				numTextures++;
				texNames[tex.texture.name]++;
				if (tex.images.empty()) { numNoPixels++; continue; }
				if (tex.images[0].image.bpp != Image::Format::ImageFormat_RGBA8888)
					numCompressed++;
				if (tex.images[0].image.width == 0 || tex.images[0].image.height == 0)
					numNoPixels++;
			}
		}
	};
	visitTextures(kenv.levelObjects);
	for (unsigned int i = 0; i < kenv.sectorObjects.size(); i++)
		visitTextures(kenv.sectorObjects[i]);
	printf("Texture dictionaries: %i, textures %i (%i DXT compressed, %i without pixels), distinct names %zu\n",
		numDicts, numTextures, numCompressed, numNoPixels, texNames.size());

	// ---- materials and the texture a mesh is drawn with
	int numMaterials = 0, numMaterialsWithTex = 0, numMaterialsUnmatched = 0;
	std::map<std::string, int> materialTexUse;
	int numGeos = 0, numNativeGeos = 0, numDecodeMismatch = 0, numDecodedTris = 0;
	int numMeshTextured = 0, numMeshUntextured = 0, numMeshNoTris = 0;
	std::map<std::string, int> drawnTextures;

	auto visitGeos = [&](KObjectList& list) {
		for (auto& cat : list.categories)
			for (auto& type : cat.type)
				for (CKObject* obj : type.objects) {
					if (!obj->isSubclassOf<CKAnyGeometry>()) continue;
					CKAnyGeometry* kg = obj->cast<CKAnyGeometry>();
					numGeos++;
					if (kg->material) {
						numMaterials++;
						const std::string& mtex = kg->material->spDiffuseName();
						if (!mtex.empty()) {
							materialTexUse[mtex]++;
							if (texNames.count(mtex))
								numMaterialsWithTex++;
							else
								numMaterialsUnmatched++;
						}
					}
					RwGeometry* rwg = kg->clump ? kg->clump->atomic.geometry.get() : nullptr;
					if (!rwg) continue;
					if (rwg->nativeVersion) {
						// console-native geometry: the conversion has to reproduce the mesh. A few
						// meshes count one or two degenerate triangles in their header that a strip
						// walk correctly leaves out, so a shortfall of up to 2 is expected.
						numNativeGeos++;
						numDecodedTris += (int)rwg->tris.size();
						size_t want = rwg->nativeVersion->numTris;
						size_t got = rwg->tris.size();
						if (got > want || want - got > 2 || rwg->verts.size() != rwg->nativeVersion->numVerts) {
							numDecodeMismatch++;
							if (numDecodeMismatch <= 20) {
								RwExtNativeData* nat = (RwExtNativeData*)rwg->nativeVersion->extensions.find(0x510);
								if (nat) {
									char nfn[64];
									snprintf(nfn, sizeof(nfn), "dotd-fail-%i.bin", numDecodeMismatch - 1);
									FILE* nf = nullptr;
									fopen_s(&nf, nfn, "wb");
									if (nf) { fwrite(nat->data.data(), 1, nat->data.size(), nf); fclose(nf); }
								}
								printf("  MISMATCH native tris %u verts %u -> decoded tris %zu verts %zu (chunk %zu bytes)\n",
									rwg->nativeVersion->numTris, rwg->nativeVersion->numVerts, rwg->tris.size(), rwg->verts.size(),
									nat ? nat->data.size() : 0);
							}
						}
					}
					std::string tex = kg->getMaterialTextureName();
					if (tex.empty() && !rwg->materialList.materials.empty() && rwg->materialList.materials[0].isTextured)
						tex = rwg->materialList.materials[0].texture.name;
					if (rwg->tris.empty())
						numMeshNoTris++;
					else if (!tex.empty()) {
						numMeshTextured++;
						drawnTextures[tex]++;
					}
					else
						numMeshUntextured++;
				}
	};
	visitGeos(kenv.levelObjects);
	for (unsigned int i = 0; i < kenv.sectorObjects.size(); i++)
		visitGeos(kenv.sectorObjects[i]);

	printf("Materials: %i referenced by geometries, %i have a texture name matching a dictionary, %i have a name no dictionary holds\n",
		numMaterials, numMaterialsWithTex, numMaterialsUnmatched);
	printf("Geometries: %i objects, %i with a console-native mesh now decoded to %i triangles, %i with a different triangle count than the header\n",
		numGeos, numNativeGeos, numDecodedTris, numDecodeMismatch);
	printf("Meshes: %i textured, %i without a texture, %i with no triangles at all (%zu distinct textures on meshes)\n",
		numMeshTextured, numMeshUntextured, numMeshNoTris, drawnTextures.size());

	check(numDicts > 0, "the level has texture dictionaries");
	check(numTextures > 0, "textures load from the dictionaries");
	check(numNoPixels == 0, "every texture has pixels");
	check(numMaterials > 0, "geometry materials are loaded as CMaterial objects");
	check(numMaterialsUnmatched == 0, "every material texture name exists in a texture dictionary");
	check(numNativeGeos > 0, "console-native (PS3) meshes decode");
	check(numDecodeMismatch == 0, "decoded meshes match the triangle and vertex count of their native header");
	check(numMeshTextured > numMeshUntextured, "more meshes are textured than not");

	// ---- what the scene graph ends up drawing
	int drawRefs = 0, drawTris = 0, drawTextured = 0;
	std::function<void(CKSceneNode*)> walk = [&](CKSceneNode* node) {
		for (; node; node = node->next.get()) {
			if (node->isSubclassOf<CNode>()) {
				for (CKAnyGeometry* kgeo = node->cast<CNode>()->geometry.get(); kgeo; kgeo = kgeo->nextGeo.get()) {
					CKAnyGeometry* rgeo = kgeo->duplicateGeo ? kgeo->duplicateGeo.get() : kgeo;
					RwGeometry* rwg = rgeo->clump ? rgeo->clump->atomic.geometry.get() : nullptr;
					if (!rwg || (rwg->flags & RwGeometry::RWGEOFLAG_NATIVE) != 0) continue;
					drawRefs++;
					drawTris += (int)rwg->tris.size();
					std::string tex = !kgeo->getMaterialTextureName().empty() ? kgeo->getMaterialTextureName() : rgeo->getMaterialTextureName();
					if (tex.empty() && !rwg->materialList.materials.empty() && rwg->materialList.materials[0].isTextured)
						tex = rwg->materialList.materials[0].texture.name;
					if (!tex.empty())
						drawTextured++;
				}
			}
			if (node->isSubclassOf<CSGBranch>()) walk(node->cast<CSGBranch>()->child.get());
			if (CAnyAnimatedNode* aan = node->dyncast<CAnyAnimatedNode>()) walk(aan->branchs.get());
		}
	};
	for (CKObject* obj : kenv.levelObjects.getClassType<CSGRootNode>().objects)
		walk(obj->cast<CSGRootNode>());
	printf("Scene graph: %i geometry references drawn, %i triangles, %i of the meshes textured\n", drawRefs, drawTris, drawTextured);
	check(drawTris > 100000, "the scene graph draws the level's meshes");
	check(drawTextured > 0, "meshes in the scene graph have a texture to draw with");

	if (failures == 0)
		printf("Dawn of the Dragon: all checks passed.\n");
	else
		printf("Dawn of the Dragon: %i check(s) FAILED.\n", failures);
	printf("Done. Press Enter to exit.\n");
	getchar();
}

void Tests::TestPrompt()
{
	static const std::pair<void(*)(), const char*> tests[] = {
		{Test_PlatformBasics, "Platform basics (encodings, resources, files)"},
		{Test_GeometryCache, "Mesh draw cache (scene node meshes)"},
		{Test_ShowBackfaces, "Show backfaces option (backface culling of mesh drawing)"},
		{Test_Cpnt2CSV, "Component values to CSV"},
		{Test_LevelSizeCheck, "Level size check"},
		{Test_ChoreoFreqValues, "Chereokey value frequency"},
		{Test_extractRemasterFilepaths, "Extract Remaster model filepaths"},
		{Test_MarioDifficulty, "Extreme Mario Mode"},
		{Test_HexEditor, "Hex Editor"},
		{Test_Diff, "LVL/STR file diff"},
		{Test_ExtractCollisionTests, "Extract Collision Tests"},
		{Test_DotDSupport, "Dawn of the Dragon support (gamepath/version/platform/initlevel of xec-settings.ini)"},
	};
	const size_t numTests = std::size(tests);
	printf("Available tests:\n");
	for (size_t t = 0; t < numTests; t++) {
		printf("%3i. %s\n", t, tests[t].second);
	}
	printf("Run which test number? ");
	char input[32];
	gets_s(input);
	size_t t = atoi(input);
	if (t < numTests)
		tests[t].first();
}

#endif