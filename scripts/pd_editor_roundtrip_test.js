const fs = require("fs");
const vm = require("vm");

const htmlPath = process.argv[2] || "Source/PdPatchEditor.html";
const html = fs.readFileSync(htmlPath, "utf8");
const repoRoot = process.cwd();
const helpPatchDir = `${repoRoot}/third_party/pure-data/extra`;
const externalPdCorpusRoot = process.env.OTHERWARE_PD_CORPUS_DIR || "";
const externalPdCorpus = externalPdCorpusRoot
  ? walkFiles(externalPdCorpusRoot, path => path.endsWith(".pd")).map(path => path.slice(externalPdCorpusRoot.length + 1)).sort()
  : [];
const match = html.match(/<script>([\s\S]*?)<\/script>/);
if (!match) throw new Error("Pd editor script not found");
const objectGroupsMatch = html.match(/const pdObjectGroups=\[([\s\S]*?)\];/);
if (!objectGroupsMatch) throw new Error("Pd object catalogue not found");
const catalogueLabels = new Set(["Control", "Data / lists", "Math", "Audio I/O", "Audio sources", "Audio math",
  "Filters / delay", "FFT / arrays", "MIDI / OSC / network", "Canvas / UI", "Files / panels",
  "Bundled extras", "Internal / advanced", "Project abstractions"]);
const editorCatalogueObjects = new Set();
for (const item of objectGroupsMatch[1].matchAll(/"([^"]+)"/g)) {
  const name = item[1];
  if (!catalogueLabels.has(name)) editorCatalogueObjects.add(name);
}
function walkFiles(root, predicate, out = []) {
  if (!fs.existsSync(root)) return out;
  for (const entry of fs.readdirSync(root, { withFileTypes: true })) {
    const path = `${root}/${entry.name}`;
    if (entry.isDirectory()) walkFiles(path, predicate, out);
    else if (predicate(path)) out.push(path);
  }
  return out;
}
const pdSourceClassNames = new Set();
for (const file of walkFiles(`${repoRoot}/third_party/pure-data/src`, p => /\.(c|h)$/.test(p))
  .concat(walkFiles(`${repoRoot}/third_party/pure-data/extra`, p => /\.(c|h)$/.test(p)))) {
  const source = fs.readFileSync(file, "utf8");
  for (const item of source.matchAll(/class_new\s*\(\s*gensym\s*\(\s*"([^"]+)"/g)) pdSourceClassNames.add(item[1]);
  for (const item of source.matchAll(/class_addcreator\s*\([^;]*?gensym\s*\(\s*"([^"]+)"/gs)) pdSourceClassNames.add(item[1]);
  if (/class_addcreator\s*\([^;]*?&s_list\b/s.test(source)) pdSourceClassNames.add("list");
}
const missingCatalogueClasses = [...pdSourceClassNames].filter(name => !editorCatalogueObjects.has(name)).sort();
if (missingCatalogueClasses.length) {
  throw new Error("Pd object catalogue is missing source classes: " + missingCatalogueClasses.join(", "));
}
if (!/\.box rect\.portHit\{[^}]*fill:transparent;stroke:none/.test(html)) {
  throw new Error("Pd port hit targets must override generic box rect styling and stay invisible");
}
if (!/\.box rect\.port\{[^}]*stroke:#151915/.test(html)) {
  throw new Error("Pd visible ports must override generic box rect styling");
}
if (/if\(!editMode\)\{[\s\S]*?canInlineEditObject\(i\)\)\{beginInlineEdit/.test(html)) {
  throw new Error("Pd run mode should not enter inline text editing on object clicks");
}
if (!/if\(guiDrag\)\{[\s\S]*else if\(editMode&&canInlineEditObject\(g\.index\)\)beginInlineEdit/.test(html)) {
  throw new Error("Pd run mode GUI mouse-up should not enter inline text editing");
}
if (!/if\(editMode\)\{\s*addMenuAction\("Edit text"[\s\S]*?addMenuAction\("Delete"/.test(html)) {
  throw new Error("Pd run mode context menu should hide edit-only object actions");
}
if (!/addMenuAction\("Edit text",canInlineEditObject\(menuTarget\),/.test(html) || !/function beginInlineEdit\(i,screenX=null,selectAll=true\)\{\s*if\(i<0\|\|i>=objects\.length\)return;\s*if\(!canInlineEditObject\(i\)\)return;/.test(html)) {
  throw new Error("Pd inline text editing should be gated to text-editable objects");
}
if (!/addMenuAction\("Delete cord",editMode,/.test(html)) {
  throw new Error("Pd run mode context menu should not delete patch cords");
}
if (!/if\(editMode&&!menuSearch\.value\.trim\(\)\)/.test(html) || !/if\(editMode\)\{\s*const groups=filteredObjectGroups/.test(html)) {
  throw new Error("Pd run mode context menu should not expose object creation");
}
if (!/editMode&&mod&&evt\.key==="1"/.test(html) || !/editMode&&selectedSet\.size>0&&\(evt\.key==="Backspace"\|\|evt\.key==="Delete"\)/.test(html)) {
  throw new Error("Pd run mode keyboard shortcuts should not mutate the patch");
}
if (!/editMode&&selectedSet\.size===1&&\(key==="e"\|\|evt\.key==="Enter"\|\|evt\.key==="Return"\)&&canInlineEditObject\(selected\)/.test(html)) {
  throw new Error("Pd E/Return shortcut should only edit text-editable objects");
}
if (!/if\(editMode&&selectedSet\.size>0\)\{[\s\S]*?ArrowLeft/.test(html)) {
  throw new Error("Pd run mode arrow keys should pan instead of moving selected objects");
}
if (!/document\.getElementById\("addPicked"\)\.onclick=\(\)=>\{\s*if\(!editMode\)return;/.test(html)) {
  throw new Error("Pd run mode object picker should not create objects");
}
if (!/document\.getElementById\("clear"\)\.onclick=\(\)=>\{if\(!editMode\)return;/.test(html)) {
  throw new Error("Pd run mode clear button should not mutate the patch");
}
if (!/function toggleSource\(button\)\{\s*if\(!editMode\)return;/.test(html)) {
  throw new Error("Pd run mode source editor should not mutate the patch");
}
if (!/function openSourceObject\(i\)\{[\s\S]*if\(!editMode\)return false;[\s\S]*if\(!isArrayLike\(objects\[i\]\)&&!isTextStore\(objects\[i\]\)\)return false;/.test(html)) {
  throw new Error("Pd run mode should not open editable array/text source panes");
}
if (!/function openPropertiesForObject\(i\)\{\s*if\(!editMode\)return false;/.test(html) || !/document\.getElementById\("propApply"\)\.onclick=\(\)=>\{\s*if\(!editMode\)return;/.test(html)) {
  throw new Error("Pd run mode GUI properties should not mutate the patch");
}
if (!/document\.getElementById\("undo"\)\.onclick=\(\)=>\{if\(editMode\)undoPatch\(\);/.test(html) || !/document\.getElementById\("redo"\)\.onclick=\(\)=>\{if\(editMode\)redoPatch\(\);/.test(html)) {
  throw new Error("Pd run mode undo/redo buttons should not mutate the patch");
}
if (!/editMode&&mod&&key==="z"&&evt\.shiftKey/.test(html) || !/editMode&&mod&&key==="z"/.test(html)) {
  throw new Error("Pd run mode undo/redo shortcuts should not mutate the patch");
}
if (!/function closeSourceEditor\(applyChanges\)/.test(html) || !/const appliedSource=!on&&sourceVisible&&closeSourceEditor\(true\);[\s\S]*if\(appliedSource\)emit\(\);/.test(html)) {
  throw new Error("entering Pd run mode should commit and close source edits");
}
if (!/if\(!editMode\)\{[\s\S]*hideObjectMenu\(\);closeProperties\(\);/.test(html)) {
  throw new Error("entering Pd run mode should dismiss edit panels");
}
if (!/function setTool\(t\)\{\s*if\(!editMode&&t!=="select"\)t="select";/.test(html) || !/if\(!editMode\)\{[\s\S]*setTool\("select"\);/.test(html)) {
  throw new Error("Pd run mode should force the toolbar back to select and reject creation tools");
}
if (!/button\[data-tool\]"\)\.forEach\(b=>b\.onclick=\(\)=>\{if\(editMode\|\|b\.dataset\.tool==="select"\)setTool\(b\.dataset\.tool\);\}\)/.test(html)) {
  throw new Error("Pd run mode toolbar buttons should not activate object creation tools");
}
if (!/if\(edit\)\{\s*if\(canInlineEditObject\(selected\)\)beginInlineEdit\(selected\);\s*else if\(isArrayLike\(objects\[selected\]\)\|\|isTextStore\(objects\[selected\]\)\)openSourceObject\(selected\);/.test(html)) {
  throw new Error("Pd creation should open inline editing only for text objects and source editing for storage objects");
}
if (!/function clearTransientEditorState\(\)\{[\s\S]*editing=-1;runtimeValueEditing=false;gopValueEditing=null;scalarValueEditing=null;inlineEdit\.style\.display="none";sourceVisible=false;editingSourceObject=-1;/.test(html)
    || !/function parsePatch\(text, addDefault=true\)\{\s*objects=\[\];[\s\S]*clearTransientEditorState\(\);/.test(html)) {
  throw new Error("Pd parse/load should clear transient inline and source editor state");
}
if (!/function clearTransientEditorState\(\)\{[\s\S]*closeProperties\(\);/.test(html)) {
  throw new Error("Pd parse/load should close stale GUI properties panels");
}
if (!/function clearTransientEditorState\(\)\{[\s\S]*patchCord=null;marquee=null;dragging=null;guiDrag=null;gopGuiDrag=null;arrayDrag=null;scalarPointDrag=null;panning=null;clickEditCandidate=null;[\s\S]*hideObjectMenu\(\);/.test(html)) {
  throw new Error("Pd parse/load should clear stale gestures and context menus");
}
if (!/document\.getElementById\("clear"\)\.onclick=\(\)=>\{if\(!editMode\)return;clearTransientEditorState\(\);/.test(html)) {
  throw new Error("Pd clear should reset stale panels and gestures before emptying the patch");
}
if (!/function leaveSubpatchCanvas\(\)\{\s*if\(!canvasStack\.length\)return false;\s*let childText=sourceVisible&&editingSourceObject<0\?sourceText\.value:serialize\(\);[\s\S]*closeSourceEditor\(true\);[\s\S]*closeSourceEditor\(false\);/.test(html)) {
  throw new Error("Pd leave-subpatch should commit or close source editor state before restoring parent");
}

const mainSourcePath = `${repoRoot}/Source/Main.cpp`;
if (fs.existsSync(mainSourcePath)) {
  const mainSource = fs.readFileSync(mainSourcePath, "utf8");
  const objectNameMatch = mainSource.match(/static juce::String objectNameFromPdObjectLine[\s\S]*?\n    \}/);
  const declaredPathsMatch = mainSource.match(/static juce::StringArray declaredPdPaths[\s\S]*?\n    \}/);

  if (!objectNameMatch || !/pdAtoms\s*\(/.test(objectNameMatch[0]) || !/unescapePdAtom\s*\(/.test(objectNameMatch[0])) {
    throw new Error("Pd abstraction port scanner must parse object names with Pd atom escaping");
  }

  if (objectNameMatch && /StringArray::fromTokens/.test(objectNameMatch[0])) {
    throw new Error("Pd abstraction port scanner regressed to whitespace tokenization");
  }

  if (!declaredPathsMatch || !/pdAtoms\s*\(\s*body\s*\)/.test(declaredPathsMatch[0])) {
    throw new Error("Pd window metadata declared-path parser must use Pd atom parsing, not whitespace splitting");
  }

  if (!declaredPathsMatch || !/stripPdFormatHints\s*\(/.test(declaredPathsMatch[0])) {
    throw new Error("Pd window metadata declared-path parser must ignore Pd object width hints");
  }

  if (!declaredPathsMatch || !/unescapePdAtom\s*\(\s*tokens\[\+\+i\]\s*\)/.test(declaredPathsMatch[0])) {
    throw new Error("Pd window metadata declared-path parser must unescape Pd path atoms");
  }

  if (/declaredPdPaths[\s\S]*?StringArray::fromTokens\s*\(\s*body/.test(declaredPathsMatch[0])) {
    throw new Error("Pd window metadata declared-path parser regressed to whitespace tokenization");
  }

  if (!/tokens\.size\(\) <= 4 \|\| tokens\[4\] != "declare"/.test(declaredPathsMatch[0])
      || !/tokens\.size\(\) > 4 && tokens\[4\] == "declare"/.test(mainSource)) {
    throw new Error("Pd window metadata should only treat the actual [declare] object as a declaration");
  }

  if (!/forEachDeclaredPdProjectDirectory[\s\S]*canResolveWithoutProjectDirectory[\s\S]*startsWith\s*\(\s*pdStdPathPrefix\s*\)[\s\S]*File::isAbsolutePath/.test(mainSource)) {
    throw new Error("Pd window metadata should resolve declared -stdpath/absolute paths even without a project folder");
  }

  if (!/abstractionNamesForPatchProject[\s\S]*forEachDeclaredPdProjectDirectory/.test(mainSource)
      || !/abstractionPortsForPatchProject[\s\S]*forEachDeclaredPdProjectDirectory/.test(mainSource)
      || !/abstractionSourcesForPatchProject[\s\S]*forEachDeclaredPdProjectDirectory/.test(mainSource)
      || !/helpPatchSourcesForPatchProject[\s\S]*forEachDeclaredPdProjectDirectory/.test(mainSource)) {
    throw new Error("Pd window metadata loaders should all share declared-path directory resolution");
  }

  const abstractionPortsMatch = mainSource.match(/static void addAbstractionPortsFromDirectory[\s\S]*?\n    \}/);
  const abstractionSourcesMatch = mainSource.match(/static void addAbstractionSourcesFromDirectory[\s\S]*?\n    \}/);
  if (!abstractionPortsMatch || !/getProperties\(\)\.contains/.test(abstractionPortsMatch[0])
      || !abstractionSourcesMatch || !/getProperties\(\)\.contains/.test(abstractionSourcesMatch[0])) {
    throw new Error("Pd abstraction metadata should keep the first matching abstraction in Pd search order");
  }
}

class FakeClassList {
  constructor() { this.values = new Set(); }
  add(value) { this.values.add(value); }
  remove(value) { this.values.delete(value); }
  contains(value) { return this.values.has(value); }
  toggle(value, force) {
    const on = force === undefined ? !this.values.has(value) : !!force;
    if (on) this.add(value); else this.remove(value);
    return on;
  }
}

class FakeElement {
  constructor(tag = "div", id = "") {
    this.tagName = tag.toUpperCase();
    this.id = id;
    this.children = [];
    this.attributes = {};
    this.style = {};
    this.classList = new FakeClassList();
    this.dataset = {};
    this.value = "";
    this.textContent = "";
    this.clientWidth = 900;
    this.clientHeight = 620;
    this.offsetWidth = 268;
    this.offsetHeight = 360;
    this.listeners = {};
  }
  appendChild(child) { this.children.push(child); return child; }
  removeChild(child) { this.children = this.children.filter(v => v !== child); return child; }
  get firstChild() { return this.children[0] || null; }
  setAttribute(key, value) {
    this.attributes[key] = String(value);
    if (key === "class") {
      this.className = String(value);
      String(value).split(/\s+/).filter(Boolean).forEach(v => this.classList.add(v));
    }
    if (key.startsWith("data-")) this.dataset[key.slice(5)] = String(value);
  }
  getAttribute(key) { return this.attributes[key] || ""; }
  getContext(type) {
    if (type !== "2d") return null;
    return { font: "", measureText: text => ({ width: String(text || "").length * 7.8 }) };
  }
  addEventListener(type, handler) {
    if (!this.listeners[type]) this.listeners[type] = [];
    this.listeners[type].push(handler);
  }
  dispatchEvent(event) {
    (this.listeners[event.type] || []).forEach(handler => handler(event));
    return true;
  }
  focus() {}
  select() {}
  setSelectionRange(start, end) { this.selectionStart = start; this.selectionEnd = end; }
  closest() { return null; }
  get options() {
    const out = [];
    const walk = node => {
      if (node.tagName === "OPTION") out.push(node);
      (node.children || []).forEach(walk);
    };
    this.children.forEach(walk);
    return out;
  }
  querySelectorAll(selector) {
    const out = [];
    const wantClass = selector.startsWith(".") ? selector.slice(1) : "";
    const walk = node => {
      const classes = String(node.className || "").split(/\s+/).filter(Boolean);
      if (wantClass && classes.includes(wantClass)) out.push(node);
      (node.children || []).forEach(walk);
    };
    this.children.forEach(walk);
    return out;
  }
}

const elements = new Map();
const ids = ["patchsvg", "status", "sourceText", "objectSearch", "objectPick", "objectMenu",
  "menuSearch", "menuList", "inlineEdit", "props", "editMode", "parentPatch", "addPicked",
  "undo", "redo", "helpObj", "source", "clear", "fit", "propTitle", "propReceive",
  "propSend", "propLabel", "propWidth", "propHeight", "propMin", "propMax", "propSteps",
  "propInit", "propSteady", "propBg", "propFg", "propLabelColor", "propLabelX", "propLabelY",
  "propFontFace", "propFontSize", "propApply", "propClose", "propLc"];

ids.forEach(id => elements.set(id, new FakeElement(id === "patchsvg" ? "svg" : "div", id)));
elements.get("objectPick").tagName = "SELECT";
elements.get("objectSearch").tagName = "INPUT";
elements.get("menuSearch").tagName = "INPUT";
elements.get("sourceText").tagName = "TEXTAREA";
elements.get("inlineEdit").tagName = "INPUT";

const svg = elements.get("patchsvg");
svg.createSVGPoint = () => ({ x: 0, y: 0, matrixTransform() { return { x: this.x, y: this.y }; } });
svg.getScreenCTM = () => ({ inverse: () => ({}) });

const document = {
  body: new FakeElement("body", "body"),
  listeners: {},
  getElementById(id) {
    if (!elements.has(id)) elements.set(id, new FakeElement("div", id));
    return elements.get(id);
  },
  createElement(tag) { return new FakeElement(tag); },
  createElementNS(_ns, tag) { return new FakeElement(tag); },
  querySelectorAll() { return []; },
  addEventListener(type, handler) {
    if (!this.listeners[type]) this.listeners[type] = [];
    this.listeners[type].push(handler);
  },
  dispatchEvent(event) {
    (this.listeners[event.type] || []).forEach(handler => handler(event));
    return true;
  }
};

const context = {
  console,
  backendEvents: [],
  document,
  window: {
    innerWidth: 1200,
    __JUCE__: { initialisationData: { patch: "", abstractions: [], abstractionPorts: {}, abstractionSources: {}, helpPatchSources: {} } },
    setTimeout() {}
  }
};
context.window.window = context.window;
context.window.document = document;
context.window.__JUCE__.backend = { emitEvent(name, payload) { context.backendEvents.push({ name, payload }); } };

const tests = `
function assert(condition, message){
 if(!condition)throw new Error(message);
}
const fixture = "#N canvas 20 30 640 480 10;\\n"
 + "#X obj 20 20 r trigger, f 18;\\n"
 + "#X msg 20 60 bang, f 30;\\n"
 + "#X text 20 100 labelled comment, f 22;\\n"
 + "#X floatatom 20 140 5 0 0 0 - - - 4.5;\\n"
 + "#X obj 20 180 bng 20 250 50 0 empty importedSend fire 0 -10 0 12 #fcfcfc #000000 #000000;\\n"
 + "#X msg 20 230 \\\\; semicolon-target 0 \\\\, 0.25 5 \\\\, 0 180 5, f 36;\\n"
 + "#X msg 20 280 \\\\; first-target bang \\\\; second-target symbol go, f 42;\\n"
 + "#X msg 20 330 0 \\\\, \\\\$1 5 \\\\, 0 \\\\$2 5, f 28;\\n"
 + "#X obj 20 380 r \\\\$0-private-send, f 24;\\n"
 + "#X obj 340 20 struct other-template float x float y float amp symbol label;\\n"
 + "#X obj 340 60 drawpolygon 777 2 0 0 x y;\\n"
 + "#X obj 340 100 drawnumber amp 12 18 0;\\n"
 + "#X obj 340 140 drawsymbol label 12 32 0;\\n"
 + "#X scalar other-template 12 34 56 symbol-value;\\n"
 + "#N canvas 120 120 300 220 inner 0;\\n"
 + "#X obj 20 20 bng 20 250 50 0 empty empty go 0 -10 0 12 #fcfcfc #000000 #000000;\\n"
 + "#X msg 60 80 hello, f 12;\\n"
 + "#X coords 0 -1 1 1 180 120 1 10 10;\\n"
 + "#X restore 240 60 pd inner;\\n"
 + "#X connect 0 0 1 0;\\n";
parsePatch(fixture,false);
const out = serialize();
assert(out.includes("#X obj 20 20 r trigger, f 18;"), "object width hint did not round-trip");
assert(out.includes("#X msg 20 60 bang, f 30;"), "message width hint did not round-trip");
assert(out.includes("#X text 20 100 labelled comment, f 22;"), "comment width hint did not round-trip");
assert(out.includes("#X msg 20 230 \\\\; semicolon-target 0 \\\\, 0.25 5 \\\\, 0 180 5, f 36;"), "semicolon message did not round-trip as Pd syntax");
assert(out.includes("#X msg 20 280 \\\\; first-target bang \\\\; second-target symbol go, f 42;"), "multi-send semicolon message did not round-trip as Pd syntax");
assert(out.includes("#X msg 20 330 0 \\\\, \\\\$1 5 \\\\, 0 \\\\$2 5, f 28;"), "dollar-variable message did not round-trip as Pd syntax");
assert(out.includes("#X obj 20 380 r \\\\$0-private-send, f 24;"), "$0 receiver object did not round-trip as Pd syntax");
assert(out.includes("importedSend"), "imported GUI send name was not preserved");
assert(out.includes("#X obj 340 20 struct other-template float x float y float amp symbol label;"), "struct definition did not round-trip");
assert(out.includes("#X obj 340 60 drawpolygon 777 2 0 0 x y;"), "draw definition did not round-trip");
assert(out.includes("#X obj 340 100 drawnumber amp 12 18 0;"), "drawnumber definition did not round-trip");
assert(out.includes("#X obj 340 140 drawsymbol label 12 32 0;"), "drawsymbol definition did not round-trip");
assert(out.includes("#X scalar other-template 12 34 56 symbol-value;"), "scalar line did not round-trip");
assert(out.includes("#X coords 0 -1 1 1 180 120 1 10 10;"), "GOP coords did not round-trip");
const scalar = objects.find(o => o.sourceKind === "scalar");
assert(scalar && scalar.scalarName === "other-template" && displayText(scalar, canonicalName(scalar)) === "scalar other-template", "scalar object was not represented clearly");
const values = scalarValueMap(scalar);
assert(values.x === 12 && values.y === 34 && values.amp === 56 && values.label === "symbol-value", "scalar values were not mapped through struct fields");
const shape = scalarShapeFromDrawObject(objects.find(o => canonicalName(o) === "drawpolygon"), values);
assert(shape && shape.points.length === 2 && shape.points[1].x === 12 && shape.points[1].y === 34, "scalar drawpolygon was not resolved from template values");
const numberLabel = scalarTextFromDrawObject(objects.find(o => canonicalName(o) === "drawnumber"), values);
const symbolLabel = scalarTextFromDrawObject(objects.find(o => canonicalName(o) === "drawsymbol"), values);
assert(numberLabel && numberLabel.text === "56" && symbolLabel && symbolLabel.text === "symbol-value", "scalar text drawings were not resolved from template values");
assert(scalar.x===12&&scalar.y===34, "scalar canvas position should come from its template x/y fields");
const scalarGeometry=scalarVisualGeometry(scalar);
assert(scalarGeometry.shapes[0].points[0].x===12&&scalarGeometry.shapes[0].points[0].y===34&&scalarGeometry.shapes[0].points[1].x===24&&scalarGeometry.shapes[0].points[1].y===68, "scalar drawings should use native base-position plus template-relative coordinates");
const scalarIndex=objects.indexOf(scalar),originalScalarSource=scalarSource(scalar);
const scalarPoint=scalarGeometry.shapes[0].points[1],scalarPointHit=scalarShapeFieldAt(scalarIndex,scalarPoint);
assert(scalarPointHit&&scalarPointHit.xField==="x"&&scalarPointHit.yField==="y"&&beginScalarPointDrag(scalarPointHit,scalarPoint), "field-backed scalar drawing points should be directly draggable");
continueScalarPointDrag({x:scalarPoint.x+3,y:scalarPoint.y+4});scalarPointDrag=null;
assert(scalarValueMap(scalar).x===15&&scalarValueMap(scalar).y===38&&scalarSource(scalar).includes("#X scalar other-template 15 38"), "dragging a scalar drawing point should update its native fields");
updateScalarFromSource(scalar,originalScalarSource);syncScalarCanvasPosition(scalar);
moveDraggedObject({index:scalarIndex,x:scalar.x,y:scalar.y},10,-4);
assert(scalarSource(scalar).includes("#X scalar other-template 22 30 56 symbol-value;")&&scalar.x===22&&scalar.y===30, "displacing a scalar should update its native x/y template fields");
updateScalarFromSource(scalar,originalScalarSource);syncScalarCanvasPosition(scalar);
const displacedScalarCopy=cloneObject(scalar,999,scalar.x+8,scalar.y+6);
assert(scalarValueMap(displacedScalarCopy).x===20&&scalarValueMap(displacedScalarCopy).y===40&&scalarSource(displacedScalarCopy).includes("#X scalar other-template 20 40"), "offset scalar copies should displace template coordinates rather than a detached editor box");
const ampLabel=scalarVisualGeometry(scalar).labels.find(label=>label.field==="amp"),ampHit=scalarFieldAt(scalarIndex,{x:ampLabel.x+1,y:ampLabel.y});
assert(beginScalarNumberDrag(ampHit,{x:ampLabel.x,y:ampLabel.y}), "drawnumber fields should begin native vertical dragging");
continueScalarPointDrag({x:ampLabel.x,y:ampLabel.y-5});scalarPointDrag=null;
assert(scalarValueMap(scalar).amp===61, "vertical drawnumber dragging should adjust the underlying float field");
setScalarFieldValue(scalar,"amp",56);
assert(ampHit&&beginScalarValueEdit(ampHit), "drawnumber fields should open direct scalar editing");
inlineEdit.value="72.5";commitInlineEdit(true);
assert(scalarValueMap(scalar).amp===72.5&&scalarSource(scalar).includes("#X scalar other-template 12 34 72.5 symbol-value;"), "drawnumber editing should update the scalar's native float field");
const labelField=scalarVisualGeometry(scalar).labels.find(label=>label.field==="label"),labelHit=scalarFieldAt(scalarIndex,{x:labelField.x+1,y:labelField.y});
assert(labelHit&&beginScalarValueEdit(labelHit), "drawsymbol fields should open direct scalar editing");
inlineEdit.value="new label";commitInlineEdit(true);
assert(scalarValueMap(scalar).label==="new label"&&scalarSource(scalar).includes("new\\ label"), "drawsymbol editing should update and escape the scalar's native symbol field");
updateScalarFromSource(scalar,originalScalarSource);syncScalarCanvasPosition(scalar);
const struct = objects.find(o => canonicalName(o) === "struct");
assert(struct && isDataStructureObject(struct) && displayText(struct, "struct").startsWith("data struct other-template"), "struct object was not represented clearly");
const semicolonMessage = objects.find(o => o.kind === "msg" && String(o.text || "").includes("semicolon-target"));
assert(semicolonMessage && semicolonMessage.text.startsWith("; semicolon-target") && semicolonMessage.formatHints === "f 36", "semicolon message was not editable/readable after import");
const multiSendMessage = objects.find(o => o.kind === "msg" && String(o.text || "").includes("second-target"));
assert(multiSendMessage && multiSendMessage.text === "; first-target bang ; second-target symbol go" && multiSendMessage.formatHints === "f 42", "multi-send semicolon message was not editable/readable after import");
const dollarMessage = objects.find(o => o.kind === "msg" && String(o.text || "").includes("$1"));
assert(dollarMessage && dollarMessage.text === "0 , $1 5 , 0 $2 5" && dollarMessage.formatHints === "f 28", "dollar-variable message was not editable/readable after import");
const privateReceiver = objects.find(o => o.kind === "obj" && String(o.text || "").includes("$0-private-send"));
assert(privateReceiver && privateReceiver.text === "r $0-private-send" && privateReceiver.formatHints === "f 24", "$0 receiver was not editable/readable after import");
const escapedAtomFixture = "#N canvas 20 30 420 220 10;\\n"
 + "#X obj 20 20 symbol one\\\\ word;\\n"
 + "#X msg 20 70 list alpha\\\\ beta gamma;\\n"
 + "#X text 20 120 comment\\\\ atom tail, f 18;\\n";
parsePatch(escapedAtomFixture,false);
const escapedAtomOut = serialize();
assert(escapedAtomOut.includes("#X obj 20 20 symbol one\\\\ word;"), "object escaped-space atom did not round-trip");
assert(escapedAtomOut.includes("#X msg 20 70 list alpha\\\\ beta gamma;"), "message escaped-space atom did not round-trip");
assert(escapedAtomOut.includes("#X text 20 120 comment\\\\ atom tail, f 18;"), "comment escaped-space atom did not round-trip");
beginInlineEdit(0);
inlineEdit.value = "symbol one word edited";
commitInlineEdit(true);
const editedEscapedAtomOut = serialize();
assert(editedEscapedAtomOut.includes("#X obj 20 20 symbol one word edited"), "edited escaped-space object did not serialize the visible edit");
assert(!editedEscapedAtomOut.includes("#X obj 20 20 symbol one\\\\ word;"), "edited escaped-space object incorrectly reused stale raw Pd body");
beginInlineEdit(0);
inlineEdit.value = "symbol one word";
commitInlineEdit(true);
const reeditedEscapedAtomOut = serialize();
assert(reeditedEscapedAtomOut.includes("#X obj 20 20 symbol one word"), "re-edited escaped-space object did not serialize the typed visible words");
assert(!reeditedEscapedAtomOut.includes("#X obj 20 20 symbol one\\\\ word;"), "re-edited escaped-space object resurrected stale raw Pd atom escaping");
const escapedSyntaxFixture = "#N canvas 20 30 420 220 10;\\n"
 + "#X msg 20 20 path C:\\\\\\\\tmp\\\\\\\\file;\\n"
 + "#X msg 20 60 list item\\\\,still-one item\\\\;still-one;\\n";
parsePatch(escapedSyntaxFixture,false);
const escapedSyntaxOut = serialize();
assert(escapedSyntaxOut.includes("#X msg 20 20 path C:\\\\\\\\tmp\\\\\\\\file;"), "literal backslash atoms did not round-trip");
assert(escapedSyntaxOut.includes("#X msg 20 60 list item\\\\,still-one item\\\\;still-one;"), "escaped comma/semicolon atoms did not round-trip");
parsePatch("#N canvas 20 30 420 220 10;\\n#X obj 20 20 trigger bang symbol\\\\ value float;\\n",false);
assert(objects[0].text === "trigger bang symbol value float", "escaped-space trigger was not readable after import");
assert(objectArity(objects[0])[1] === 3, "escaped-space trigger atom was incorrectly counted as multiple outlets");
beginInlineEdit(0);
assert(inlineEdit.value === "trigger bang symbol\\\\ value float", "inline editor should preserve the Pd-escaped source body for escaped atoms");
commitInlineEdit(true);
assert(objectArity(objects[0])[1] === 3, "committing untouched escaped-space trigger split the escaped atom");
assert(serialize().includes("#X obj 20 20 trigger bang symbol\\\\ value float"), "untouched escaped-space trigger did not serialize with the original atom boundary");
parsePatch("#N canvas 20 30 420 220 10;\\n#X obj 20 20 osc~ 220;\\n",false);
beginInlineEdit(0);
inlineEdit.value = "trigger bang symbol\\\\ value float";
commitInlineEdit(true);
assert(objects[0].text === "trigger bang symbol value float", "typed escaped-space trigger was not readable after edit");
assert(objectArity(objects[0])[1] === 3, "typed escaped-space trigger atom was incorrectly counted as multiple outlets");
assert(serialize().includes("#X obj 20 20 trigger bang symbol\\\\ value float"), "typed escaped-space trigger did not serialize with the intended atom boundary");
const complexDataFixture = "#N canvas 20 30 760 560 10;\\n"
 + "#X obj 40 30 struct complex-template float x float y float a float b symbol label;\\n"
 + "#X obj 40 70 filledpolygon 900 99 2 0 0 x 0 x y 0 y;\\n"
 + "#X obj 40 110 filledcurve 550 44 2 0 0 x b y a 0;\\n"
 + "#X obj 40 150 drawcurve 100 3 0 b x a y;\\n"
 + "#X obj 40 190 drawtext label 8 24 0 name label;\\n"
 + "#X obj 40 230 plot a 600 1 0 0 20;\\n"
 + "#X scalar complex-template 30 40 12 28 bright;\\n"
 + "#N canvas 100 100 420 320 nested-data 0;\\n"
 + "#X obj 30 30 struct nested-template float x float y symbol word;\\n"
 + "#X obj 30 70 drawpolygon 222 1 0 0 x y;\\n"
 + "#X obj 30 110 drawsymbol word 4 18 0;\\n"
 + "#X scalar nested-template 7 11 inner-word;\\n"
 + "#X restore 360 80 pd nested-data;\\n";
parsePatch(complexDataFixture,false);
const complexOut = serialize();
assert(complexOut.includes("#X obj 40 70 filledpolygon 900 99 2 0 0 x 0 x y 0 y;"), "filledpolygon data drawing did not round-trip");
assert(complexOut.includes("#X obj 40 110 filledcurve 550 44 2 0 0 x b y a 0;"), "filledcurve data drawing did not round-trip");
assert(complexOut.includes("#X obj 40 150 drawcurve 100 3 0 b x a y;"), "drawcurve data drawing did not round-trip");
assert(complexOut.includes("#X obj 40 190 drawtext label 8 24 0 name label;"), "drawtext data drawing did not round-trip");
assert(complexOut.includes("#X obj 40 230 plot a 600 1 0 0 20;"), "plot data drawing did not round-trip");
assert(complexOut.includes("#X scalar complex-template 30 40 12 28 bright;"), "complex scalar did not round-trip");
const complexScalar = objects.find(o => o.scalarName === "complex-template");
const complexValues = scalarValueMap(complexScalar);
assert(complexValues.x === 30 && complexValues.y === 40 && complexValues.a === 12 && complexValues.b === 28 && complexValues.label === "bright", "complex scalar fields were not mapped");
const filledShape = scalarShapeFromDrawObject(objects.find(o => canonicalName(o) === "filledpolygon"), complexValues);
const curveShape = scalarShapeFromDrawObject(objects.find(o => canonicalName(o) === "drawcurve"), complexValues);
const textShape = scalarTextFromDrawObject(objects.find(o => canonicalName(o) === "drawtext"), complexValues);
assert(filledShape && filledShape.filled && filledShape.points.length === 4, "filledpolygon was not resolved for scalar preview");
assert(curveShape && !curveShape.filled && curveShape.points[0].y === 28, "drawcurve did not resolve scalar variables");
assert(textShape && textShape.text === "namebright", "drawtext did not resolve the native label plus scalar field text");
const nestedData = objects.find(o => o.sourceKind === "subpatch");
assert(nestedData && enterSubpatchCanvas(objects.indexOf(nestedData)), "could not enter nested data-structure subpatch");
const nestedScalar = objects.find(o => o.scalarName === "nested-template");
assert(nestedScalar && scalarValueMap(nestedScalar).word === "inner-word", "nested scalar values were not available after entering subpatch");
objects.push({ id: nextId++, kind: "obj", x: 30, y: 150, text: "filledpolygon 111 22 1 0 0 x y" });
leaveSubpatchCanvas();
const nestedDataOut = serialize();
assert(nestedDataOut.includes("#X scalar nested-template 7 11 inner-word;"), "nested scalar did not remain serialized after editing subpatch");
assert(nestedDataOut.includes("#X obj 30 150 filledpolygon 111 22 1 0 0 x y;"), "new nested data drawing did not serialize after leaving subpatch");
const nestedArrayFixture="#N canvas 20 30 760 560 10;\\n"
 + "#X obj 30 30 struct leaf-template float x float y;\\n"
 + "#X obj 30 70 struct point-template float x float y float w list meta array accents leaf-template symbol name;\\n"
 + "#X obj 250 70 drawpolygon -d 222 1 -2 0 2 0;\\n"
 + "#X obj 250 100 drawsymbol name 3 -4 0;\\n"
 + "#X obj 250 130 plot accents 333 1 0 0 1;\\n"
 + "#X obj 30 110 struct path-template float x list notes array points point-template float y symbol label;\\n"
 + "#X obj 30 150 plot points 777 2 0 0 10;\\n"
 + "#X obj 30 190 drawtext notes 0 30 0 notes:;\\n"
 + "#X scalar path-template 10 20 root;\\n"
 + "root note 1;\\n"
 + "0 5 2 first;\\n"
 + "first meta;\\n"
 + "1 2;\\n"
 + ";\\n"
 + "10 15 4 second;\\n"
 + "second meta;\\n"
 + "3 4;\\n"
 + "5 6;\\n"
 + ";\\n"
 + ";\\n";
parsePatch(nestedArrayFixture,false);
const pathScalar=objects.find(o=>o.scalarName==="path-template"),pathValues=scalarValueMap(pathScalar);
assert(pathValues.x===10&&pathValues.y===20&&pathValues.label==="root", "array fields must not consume primitive scalar value positions");
assert(pathScalar.scalarArrays.points.length===2&&pathScalar.scalarArrays.points[0].arrays.accents.length===1&&pathScalar.scalarArrays.points[1].arrays.accents.length===2, "recursive scalar arrays were not parsed with their native terminators");
assert(pathScalar.scalarTexts.notes.join(" ")==="root note 1"&&pathScalar.scalarArrays.points[1].texts.meta.join(" ")==="second meta", "text/list fields should be parsed in template order around nested arrays");
const notesLabel=scalarVisualGeometry(pathScalar).labels.find(label=>label.field==="notes"),notesHit=scalarFieldAt(objects.indexOf(pathScalar),{x:notesLabel.x+2,y:notesLabel.y});
assert(notesLabel.text==="notes:root note 1"&&notesHit&&beginScalarValueEdit(notesHit), "drawtext should display and directly edit scalar list fields");
inlineEdit.value="updated note";commitInlineEdit(true);
assert(pathScalar.scalarTexts.notes.join(" ")==="updated note", "editing drawtext should update the native scalar text field");
const pathPlot=scalarVisualGeometry(pathScalar).shapes.find(shape=>shape.plot);
assert(pathPlot&&pathPlot.points.length===2&&pathPlot.points[0].x===10&&pathPlot.points[0].y===25&&pathPlot.points[1].x===20&&pathPlot.points[1].y===35, "plot should render nested scalar-array x/y fields in owner coordinates");
const pathGeometry=scalarVisualGeometry(pathScalar),elementLabels=pathGeometry.labels.filter(label=>label.field==="name");
assert(elementLabels.length===2&&elementLabels[0].text==="first"&&elementLabels[1].text==="second"&&pathGeometry.shapes.filter(shape=>!shape.plot).length>=2, "plot scalar visibility should recursively render each element template");
const firstElementHit=scalarFieldAt(objects.indexOf(pathScalar),{x:elementLabels[0].x+1,y:elementLabels[0].y});
assert(firstElementHit&&firstElementHit.entryRef===pathScalar.scalarArrays.points[0]&&beginScalarValueEdit(firstElementHit), "nested element draw labels should target their own scalar entry");
inlineEdit.value="alpha";commitInlineEdit(true);
assert(scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[0]).name==="alpha", "editing a nested drawsymbol should update that element's native symbol field");
const deepPlot=pathGeometry.shapes.find(shape=>shape.plot&&shape.elementTemplate==="leaf-template"),deepHit=scalarShapeFieldAt(objects.indexOf(pathScalar),deepPlot.points[0]);
assert(deepHit&&deepHit.entryRef===pathScalar.scalarArrays.points[0].arrays.accents[0]&&beginScalarPointDrag(deepHit,deepPlot.points[0]), "deeply nested plot points should retain their actual element reference");
continueScalarPointDrag({x:deepPlot.points[0].x+2,y:deepPlot.points[0].y+2});scalarPointDrag=null;
assert(scalarEntryValueMap("leaf-template",pathScalar.scalarArrays.points[0].arrays.accents[0]).x===3, "deeply nested plot gestures should update the correct recursive element");
assert(pathPlot.filled&&pathPlot.outlinePoints.length===4, "a plot element w field should produce the native variable-width filled trace");
assert(scalarShapePath({points:[{x:0,y:0},{x:10,y:20},{x:20,y:0}],curve:true,filled:false}).includes(" C "), "Bezier plot style should render a smooth path rather than a polyline");
const pathIndex=objects.indexOf(pathScalar),plotHit=scalarShapeFieldAt(pathIndex,pathPlot.points[1]);
assert(plotHit&&plotHit.nested&&plotHit.entryIndex===1&&beginScalarPointDrag(plotHit,pathPlot.points[1]), "editable plot points should target their nested scalar element");
continueScalarPointDrag({x:pathPlot.points[1].x+4,y:pathPlot.points[1].y-3});scalarPointDrag=null;
assert(scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[1]).x===14&&scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[1]).y===12, "dragging a plot point should update the nested element fields");
const widenedPlot=scalarVisualGeometry(pathScalar).shapes.find(shape=>shape.plot),widthHit=scalarShapeFieldAt(pathIndex,{x:widenedPlot.points[1].x,y:widenedPlot.points[1].y-4});
assert(widthHit&&widthHit.widthSign===-1&&beginScalarPointDrag(widthHit,{x:widenedPlot.points[1].x,y:widenedPlot.points[1].y-4}), "plot width edges should be independently draggable");
continueScalarPointDrag({x:widenedPlot.points[1].x,y:widenedPlot.points[1].y-6});scalarPointDrag=null;
assert(scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[1]).w===6, "dragging the upper plot edge should update its w field");
const shiftedPlot=scalarVisualGeometry(pathScalar).shapes.find(shape=>shape.plot),shiftHit=scalarShapeFieldAt(pathIndex,shiftedPlot.points[0]);
beginScalarPointDrag(shiftHit,shiftedPlot.points[0],true);continueScalarPointDrag({x:shiftedPlot.points[0].x+2,y:shiftedPlot.points[0].y+3});scalarPointDrag=null;
assert(scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[0]).x===2&&scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[1]).x===16, "Shift-drag should move the selected and remaining plot elements together");
const membershipPlot=scalarVisualGeometry(pathScalar).shapes.find(shape=>shape.plot),membershipHit=scalarShapeFieldAt(pathIndex,membershipPlot.points[0]);
editScalarPlotMembership(membershipHit,{x:membershipHit.point.x+1,y:membershipHit.point.y});assert(pathScalar.scalarArrays.points.length===3, "Option-click to the right should insert a copied plot element");
const insertedPlot=scalarVisualGeometry(pathScalar).shapes.find(shape=>shape.plot),insertedHit=scalarShapeFieldAt(pathIndex,insertedPlot.points[1]);editScalarPlotMembership(insertedHit,{x:insertedHit.point.x-1,y:insertedHit.point.y});
assert(pathScalar.scalarArrays.points.length===2, "Option-click to the left should delete a plot element");
const finalPlot=scalarVisualGeometry(pathScalar).shapes.find(shape=>shape.plot&&shape.elementTemplate==="point-template"),wholeHit=scalarWholeDragAt(pathIndex,finalPlot.points[0]);
assert(wholeHit&&wholeHit.shape.dragEntry===pathScalar.scalarArrays.points[0]&&beginScalarWholeDrag(wholeHit,finalPlot.points[0]), "-d draw shapes should grab their owning nested scalar");
continueScalarPointDrag({x:finalPlot.points[0].x+1,y:finalPlot.points[0].y+2});scalarPointDrag=null;
assert(scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[0]).x===3&&scalarEntryValueMap("point-template",pathScalar.scalarArrays.points[0]).y===10, "dragging a -d element drawing should move that nested scalar's x/y fields");
const nestedArrayOut=serialize();
assert(nestedArrayOut.includes("#X scalar path-template 10 20 root;\\nupdated note;\\n3 10 2 alpha;\\nfirst meta;\\n3 4;\\n;\\n16 15 6 second;\\nsecond meta;\\n3 4;\\n5 6;\\n;\\n;"), "nested scalar arrays and text fields did not serialize recursively after editing");
assert(scalarShapeFromDrawObject({kind:"obj",text:"drawcurve -v hidden 777 2 0 0 10 10"},{hidden:0})===null, "draw-shape visibility fields should suppress hidden geometry");
const lockedShape=scalarShapeFromDrawObject({kind:"obj",text:"drawcurve -x 777 2 0 0 10 10"},{});
assert(lockedShape&&lockedShape.curve&&lockedShape.editable===false&&lockedShape.outlineColor===777&&pdTemplateColor(900)==="#ff0000", "draw-shape flags, curve style, and Pd data colors should use native semantics");
const lockedText=scalarTextFromDrawObject({kind:"obj",text:"drawnumber -x amount 2 3 900 value:"},{amount:4});
assert(lockedText&&lockedText.text==="value:4"&&lockedText.editable===false&&lockedText.color===900, "drawtext flags, labels, and data colors should retain native semantics");
const scopedTemplateFixture="#N canvas 20 30 640 440 10;\\n"
 + "#N canvas 100 100 420 300 templates 0;\\n"
 + "#X obj 20 20 struct scoped-point float x float y;\\n"
 + "#X obj 20 60 struct scoped-path float x float y array points scoped-point;\\n"
 + "#X obj 20 100 plot points 500 1 0 0 1;\\n"
 + "#X restore 360 40 pd templates;\\n"
 + "#X scalar scoped-path 50 60;\\n"
 + "0 2;\\n"
 + "8 12;\\n"
 + ";\\n";
parsePatch(scopedTemplateFixture,false);
const scopedScalar=objects.find(o=>o.scalarName==="scoped-path"),scopedPlot=scalarVisualGeometry(scopedScalar).shapes.find(shape=>shape.plot);
assert(scopedScalar&&scopedScalar.scalarArrays.points.length===2&&scopedPlot&&scopedPlot.points[1].x===58&&scopedPlot.points[1].y===72, "templates and plot commands inside another canvas should resolve patch-wide");
assert(serialize().includes("#X scalar scoped-path 50 60;\\n0 2;\\n8 12;\\n;"), "scalars using a scoped template should retain nested data on round-trip");
const longObject = { id: 900, kind: "obj", x: 0, y: 0, text: "very_long_custom_object_name with many arguments and symbols" };
assert(boxSize(longObject).w > boxSize({ id: 901, kind: "obj", x: 0, y: 0, text: "osc~" }).w, "object boxes did not expand to fit long contents");
const visiblePort = portRectAttrs({ id: 900, kind: "obj", x: 0, y: 0, text: "trigger bang bang" }, 0, 0, true, false);
const hitPort = portRectAttrs({ id: 900, kind: "obj", x: 0, y: 0, text: "trigger bang bang" }, 0, 0, true, true);
assert(visiblePort.width * screenScaleX() <= 4.1 && visiblePort.height * screenScaleY() <= 1.6, "visible Pd ports should stay as tiny Pd-style tabs");
assert(hitPort.width > visiblePort.width * 3.5 && hitPort.height > visiblePort.height * 7, "Pd ports should keep a larger invisible hit target for patching");
parsePatch("#N canvas 20 30 420 260 10;\\n#X obj 20 20 osc~ 220;\\n#X obj 20 90 dac~;\\n#X connect 0 0 1 0;\\n", false);
const detachedCord = connections.splice(0, 1)[0];
patchCord = makePatchCordFromPort("outlet", { index: detachedCord.from, port: detachedCord.out || 0 }, { x: 80, y: 80 }, true, detachedCord);
patchCord.detachedIndex = 0;
cancelPatchCord(false);
assert(connections.length === 0, "Escape-style cancel of a detached Pd cord should remove the cord instead of restoring it");
patchCord = makePatchCordFromPort("outlet", { index: 0, port: 0 }, { x: 80, y: 80 }, false);
cancelPatchCord(true);
assert(connections.length === 0, "cancelling a new unfinished Pd cord should not create a connection");
view = { x: 0, y: 0, w: 900, h: 620 };
easePointIntoView({ x: 890, y: 610 });
assert(view.x > 0 && view.y > 0, "dragging a Pd patch cord or marquee near the lower-right edge should gently pan the canvas");
assert(view.x <= 900 * 0.012 + 0.01 && view.y <= 900 * 0.012 + 0.01, "edge-follow panning should remain deliberately slow, not lurch across the Pd canvas");
view = { x: 0, y: 0, w: 900, h: 620 };
easePointIntoView({ x: 450, y: 310 });
assert(view.x === 0 && view.y === 0, "Pd canvas should not pan while cable/marquee dragging stays away from the edge");
assert(objectArity({ id: 901, kind: "obj", x: 0, y: 0, text: "trigger" })[1] === 2 && objectArity({ id: 901, kind: "obj", x: 0, y: 0, text: "t" })[1] === 2, "bare trigger should expose Pd's default two bang outlets");
assert(objectArity({ id: 900, kind: "msg", x: 0, y: 0, text: "hello" })[0] === 1 && objectArity({ id: 900, kind: "msg", x: 0, y: 0, text: "hello" })[1] === 1, "message boxes should expose Pd's single inlet and single outlet");
assert(objectArity({ id: 901, kind: "obj", x: 0, y: 0, text: "trigger bang float symbol" })[1] === 3, "typed trigger should expose one outlet per requested type");
assert(objectArity({ id: 902, kind: "obj", x: 0, y: 0, text: "send" })[0] === 2 && objectArity({ id: 903, kind: "obj", x: 0, y: 0, text: "send named-dest" })[0] === 1, "bare send should expose Pd's destination-name inlet while named send should not");
assert(objectArity({ id: 904, kind: "obj", x: 0, y: 0, text: "s" })[0] === 2 && objectArity({ id: 905, kind: "obj", x: 0, y: 0, text: "s named-dest" })[0] === 1, "bare s should expose Pd's destination-name inlet while named s should not");
assert(objectArity({ id: 906, kind: "obj", x: 0, y: 0, text: "send~" })[0] === 1 && objectArity({ id: 907, kind: "obj", x: 0, y: 0, text: "throw~" })[0] === 1, "signal send/throw objects should keep only their signal inlet");
assert(objectArity({ id: 908, kind: "obj", x: 0, y: 0, text: "inlet~" })[0] === 1 && objectArity({ id: 909, kind: "obj", x: 0, y: 0, text: "inlet~" })[1] === 2, "inlet~ should expose Pd's local fwd inlet/outlet as well as its signal outlet");
assert(arity["inlet~"][0] === 1 && arity["inlet~"][1] === 2, "static inlet~ arity should stay aligned with the Pd-accurate dynamic model");
const audioSubpatchWithInletTilde = "#N canvas 120 120 360 260 audio 0;\\n#X obj 20 20 inlet~;\\n#X obj 20 80 outlet~;\\n";
assert(objectArity({ id: 910, kind: "obj", x: 0, y: 0, text: "pd audio", sourceKind: "subpatch", subpatchText: audioSubpatchWithInletTilde })[0] === 1 && objectArity({ id: 911, kind: "obj", x: 0, y: 0, text: "pd audio", sourceKind: "subpatch", subpatchText: audioSubpatchWithInletTilde })[1] === 1, "subpatch arity should count inlet~/outlet~ as parent ports, not their local fwd ports");
assert(objectArity({ id: 914, kind: "obj", x: 0, y: 0, text: "pack" })[0] === 2, "bare pack should expose Pd's default two inlets");
assert(objectArity({ id: 915, kind: "obj", x: 0, y: 0, text: "unpack" })[1] === 2, "bare unpack should expose Pd's default two outlets");
assert(objectArity({ id: 916, kind: "obj", x: 0, y: 0, text: "select 7" })[0] === 2 && objectArity({ id: 917, kind: "obj", x: 0, y: 0, text: "select 7 8" })[0] === 1, "select should expose Pd's single-argument comparison inlet only for one argument");
assert(objectArity({ id: 918, kind: "obj", x: 0, y: 0, text: "route symbol" })[0] === 2 && objectArity({ id: 919, kind: "obj", x: 0, y: 0, text: "route float list" })[0] === 1, "route should expose Pd's single-argument comparison inlet only for one argument");
assert(objectArity({ id: 902, kind: "obj", x: 0, y: 0, text: "pack f f f f" })[0] === 4, "pack inlet count did not follow typed arguments");
assert(objectArity({ id: 903, kind: "obj", x: 0, y: 0, text: "unpack f f f" })[1] === 3, "unpack outlet count did not follow typed arguments");
assert(objectArity({ id: 904, kind: "obj", x: 0, y: 0, text: "list split 4" })[1] === 3, "list split should expose its three Pd outlets");
assert(objectArity({ id: 904, kind: "obj", x: 0, y: 0, text: "list store" })[0] === 1 && objectArity({ id: 904, kind: "obj", x: 0, y: 0, text: "list store" })[1] === 2, "list store should expose Pd's single inlet plus list and done-bang outlets");
assert(objectArity({ id: 905, kind: "obj", x: 0, y: 0, text: "poly 8 1" })[0] === 2 && objectArity({ id: 906, kind: "obj", x: 0, y: 0, text: "poly 8 1" })[1] === 3, "poly should expose Pd's pitch/velocity inlets and voice/pitch/velocity outlets");
registerAbstractionPorts({ voice: [3, 2], silent_voice: [0, 0] });
assert(objectArity({ id: 906, kind: "obj", x: 0, y: 0, text: "clone voice 4" })[0] === 3 && objectArity({ id: 907, kind: "obj", x: 0, y: 0, text: "clone voice 4" })[1] === 2, "clone should expose the wrapped abstraction's inlets and outlets");
assert(objectArity({ id: 908, kind: "obj", x: 0, y: 0, text: "clone 4 voice" })[0] === 3 && objectArity({ id: 909, kind: "obj", x: 0, y: 0, text: "clone 4 voice" })[1] === 2, "clone should parse Pd's count-first abstraction syntax");
assert(objectArity({ id: 910, kind: "obj", x: 0, y: 0, text: "clone -x -s 2 4 voice foo" })[0] === 3 && objectArity({ id: 911, kind: "obj", x: 0, y: 0, text: "clone -x -s 2 4 voice foo" })[1] === 2, "clone should skip Pd clone flags before finding the abstraction name");
assert(objectArity({ id: 912, kind: "obj", x: 0, y: 0, text: "clone silent_voice 8" })[0] === 1 && objectArity({ id: 913, kind: "obj", x: 0, y: 0, text: "clone silent_voice 8" })[1] === 0, "clone should keep Pd's fake control inlet when the wrapped abstraction has no inlets");
assert(objectArity({ id: 907, kind: "obj", x: 0, y: 0, text: "bag -u" })[0] === 2 && objectArity({ id: 908, kind: "obj", x: 0, y: 0, text: "bag -u" })[1] === 2, "bag should expose Pd's value/velocity inlets and value/aux outlets");
assert(objectArity({ id: 909, kind: "obj", x: 0, y: 0, text: "pointer point line" })[0] === 2 && objectArity({ id: 910, kind: "obj", x: 0, y: 0, text: "pointer point line" })[1] === 4, "pointer should expose set inlet plus typed/other/bang outlets");
assert(objectArity({ id: 911, kind: "obj", x: 0, y: 0, text: "vpointer shared point line" })[0] === 2 && objectArity({ id: 912, kind: "obj", x: 0, y: 0, text: "vpointer shared point line" })[1] === 4, "vpointer should expose set inlet plus named typed/other/bang outlets");
assert(objectArity({ id: 913, kind: "obj", x: 0, y: 0, text: "get point x y label" })[1] === 3, "get should expose one outlet per requested field");
assert(objectArity({ id: 914, kind: "obj", x: 0, y: 0, text: "set point x y label" })[0] === 4, "set should expose one inlet per field plus pointer inlet");
assert(objectArity({ id: 915, kind: "obj", x: 0, y: 0, text: "append point x y" })[0] === 3 && objectArity({ id: 916, kind: "obj", x: 0, y: 0, text: "append point x y" })[1] === 1, "append should expose field inlets, pointer inlet, and pointer outlet");
assert(objectArity({ id: 917, kind: "obj", x: 0, y: 0, text: "element point path" })[0] === 2 && objectArity({ id: 918, kind: "obj", x: 0, y: 0, text: "element point path" })[1] === 1, "element should expose index/parent-pointer inlets and child pointer outlet");
assert(objectArity({ id: 919, kind: "obj", x: 0, y: 0, text: "getsize point path" })[0] === 1 && objectArity({ id: 920, kind: "obj", x: 0, y: 0, text: "getsize point path" })[1] === 1, "getsize should expose pointer inlet and size outlet");
assert(objectArity({ id: 921, kind: "obj", x: 0, y: 0, text: "setsize point path" })[0] === 2 && objectArity({ id: 922, kind: "obj", x: 0, y: 0, text: "setsize point path" })[1] === 0, "setsize should expose pointer and size inlets without outlets");
assert(objectArity({ id: 970, kind: "obj", x: 0, y: 0, text: "text define -k steps" })[1] === 2, "text define should expose pointer and notify outlets");
assert(objectArity({ id: 971, kind: "obj", x: 0, y: 0, text: "text get steps" })[0] === 4 && objectArity({ id: 972, kind: "obj", x: 0, y: 0, text: "text get steps" })[1] === 2, "text get should expose line/field/count/source inlets and list/status outlets");
assert(objectArity({ id: 973, kind: "obj", x: 0, y: 0, text: "text set steps" })[0] === 4 && objectArity({ id: 974, kind: "obj", x: 0, y: 0, text: "text insert steps" })[0] === 3, "text set/insert should expose Pd's index/source inlets");
assert(objectArity({ id: 975, kind: "obj", x: 0, y: 0, text: "text delete steps" })[0] === 2 && objectArity({ id: 976, kind: "obj", x: 0, y: 0, text: "text size steps" })[0] === 2, "text delete/size should expose source inlet");
assert(objectArity({ id: 979, kind: "obj", x: 0, y: 0, text: "text search steps 0 > 2 1 < 5" })[1] === 1, "text search should expose only Pd's single result-list outlet");
assert(objectArity({ id: 977, kind: "obj", x: 0, y: 0, text: "text sequence steps" })[1] === 2 && objectArity({ id: 978, kind: "obj", x: 0, y: 0, text: "text sequence steps -w 1" })[1] === 3, "text sequence should expose optional wait outlet");
assert(objectArity({ id: 979, kind: "obj", x: 0, y: 0, text: "pipe" })[0] === 2 && objectArity({ id: 980, kind: "obj", x: 0, y: 0, text: "pipe" })[1] === 1, "bare pipe should expose Pd's default value inlet, delay inlet, and value outlet");
assert(objectArity({ id: 981, kind: "obj", x: 0, y: 0, text: "pipe f s p 250" })[0] === 4 && objectArity({ id: 982, kind: "obj", x: 0, y: 0, text: "pipe f s p 250" })[1] === 3, "typed pipe should expose one inlet/outlet per field plus delay inlet");
assert(objectArity({ id: 983, kind: "obj", x: 0, y: 0, text: "realtime" })[0] === 2 && objectArity({ id: 984, kind: "obj", x: 0, y: 0, text: "cputime" })[0] === 2, "Pd timing objects should expose start and stop bang inlets");
assert(objectArity({ id: 989, kind: "obj", x: 0, y: 0, text: "change" })[0] === 1 && objectArity({ id: 990, kind: "obj", x: 0, y: 0, text: "change" })[1] === 1, "change should expose only Pd's left inlet and changed-value outlet");
["float","f","int","i","symbol"].forEach((name, offset) => {
 assert(objectArity({ id: 991 + offset, kind: "obj", x: 0, y: 0, text: name })[0] === 2 && objectArity({ id: 996 + offset, kind: "obj", x: 0, y: 0, text: name })[1] === 1, name + " should expose Pd's hot inlet, cold storage inlet, and value outlet");
});
assert(objectArity({ id: 985, kind: "obj", x: 0, y: 0, text: "value" })[0] === 2 && objectArity({ id: 986, kind: "obj", x: 0, y: 0, text: "value named-store" })[0] === 1, "bare value should expose Pd's storage-name inlet while named value should not");
assert(objectArity({ id: 987, kind: "obj", x: 0, y: 0, text: "v" })[0] === 2 && objectArity({ id: 988, kind: "obj", x: 0, y: 0, text: "v named-store" })[0] === 1, "bare v should expose Pd's storage-name inlet while named v should not");
assert(objectArity({ id: 910, kind: "obj", x: 0, y: 0, text: "expr $f1 + $f3 ; $f2 * 2" })[0] === 3, "expr inlet count should follow referenced control variables");
assert(objectArity({ id: 911, kind: "obj", x: 0, y: 0, text: "expr $f1 + $f3 ; $f2 * 2" })[1] === 2, "expr outlet count should follow semicolon-separated expressions");
assert(objectArity({ id: 912, kind: "obj", x: 0, y: 0, text: "expr~ $v1 + $v2 ; $v3 * 0.5" })[0] === 3, "expr~ inlet count should follow referenced signal variables");
assert(objectArity({ id: 913, kind: "obj", x: 0, y: 0, text: "fexpr~ $x1[-1] + $y2[-1] ; $x3[0]" })[1] === 2, "fexpr~ outlet count should follow semicolon-separated expressions");
assert(objectArity({ id: 914, kind: "obj", x: 0, y: 0, text: "choice" })[0] === 1 && objectArity({ id: 915, kind: "obj", x: 0, y: 0, text: "choice 1" })[1] === 1, "choice should expose Pd's default inlet and one float outlet");
assert(objectArity({ id: 916, kind: "obj", x: 0, y: 0, text: "sig~" })[0] === 1 && objectArity({ id: 917, kind: "obj", x: 0, y: 0, text: "sig~ 1 2 3" })[0] === 3, "sig~ should expose one inlet per creation value");
["sqrt","exp","abs","sin","cos","tan","atan","wrap","mtof","ftom","dbtorms","rmstodb","dbtopow","powtodb"].forEach((name, offset) => {
 assert(objectArity({ id: 940 + offset, kind: "obj", x: 0, y: 0, text: name })[0] === 1 && objectArity({ id: 960 + offset, kind: "obj", x: 0, y: 0, text: name })[1] === 1, name + " should expose unary control ports");
});
assert(objectArity({ id: 955, kind: "obj", x: 0, y: 0, text: "log" })[0] === 2 && objectArity({ id: 956, kind: "obj", x: 0, y: 0, text: "log" })[1] === 1, "log should expose value and base inlets");
assert(objectArity({ id: 916, kind: "obj", x: 0, y: 0, text: "atan2" })[0] === 2, "atan2 should expose two inlets");
assert(objectArity({ id: 917, kind: "obj", x: 0, y: 0, text: "cpole~" })[0] === 4 && objectArity({ id: 918, kind: "obj", x: 0, y: 0, text: "cpole~" })[1] === 2, "cpole~ should expose complex filter ports");
assert(objectArity({ id: 919, kind: "obj", x: 0, y: 0, text: "czero~" })[0] === 4 && objectArity({ id: 920, kind: "obj", x: 0, y: 0, text: "czero~" })[1] === 2, "czero~ should expose complex filter ports");
assert(objectArity({ id: 921, kind: "obj", x: 0, y: 0, text: "rpole~" })[0] === 2 && objectArity({ id: 922, kind: "obj", x: 0, y: 0, text: "rzero_rev~" })[0] === 2, "real pole/zero filters should expose coefficient inlets");
assert(objectArity({ id: 923, kind: "obj", x: 0, y: 0, text: "biquad~" })[0] === 1 && objectArity({ id: 924, kind: "obj", x: 0, y: 0, text: "biquad~" })[1] === 1, "biquad~ should take coefficient lists through its main inlet, not expose fake coefficient inlets");
assert(objectArity({ id: 979, kind: "obj", x: 0, y: 0, text: "slop~" })[0] === 6 && objectArity({ id: 979, kind: "obj", x: 0, y: 0, text: "slop~" })[1] === 1, "slop~ should expose Pd's six signal inlets and one outlet");
assert(objectArity({ id: 923, kind: "obj", x: 0, y: 0, text: "q8_sqrt~" })[0] === 1 && objectArity({ id: 924, kind: "obj", x: 0, y: 0, text: "q8_rsqrt~" })[1] === 1, "q8 signal math aliases should expose unary signal ports");
assert(objectArity({ id: 925, kind: "obj", x: 0, y: 0, text: "wrap~" })[0] === 1 && objectArity({ id: 926, kind: "obj", x: 0, y: 0, text: "wrap~" })[1] === 1, "wrap~ should expose unary signal ports");
assert(objectArity({ id: 927, kind: "obj", x: 0, y: 0, text: "log~" })[0] === 2 && objectArity({ id: 928, kind: "obj", x: 0, y: 0, text: "log~" })[1] === 1, "log~ should expose signal and base inlets");
assert(objectArity({ id: 925, kind: "obj", x: 0, y: 0, text: "framp~" })[0] === 2 && objectArity({ id: 926, kind: "obj", x: 0, y: 0, text: "framp~" })[1] === 2, "framp~ should expose Pd's two signal inlets and two signal outlets");
assert(objectArity({ id: 927, kind: "obj", x: 0, y: 0, text: "fft~" })[0] === 2 && objectArity({ id: 928, kind: "obj", x: 0, y: 0, text: "fft~" })[1] === 2, "fft~ should expose real/imaginary signal ports");
assert(objectArity({ id: 929, kind: "obj", x: 0, y: 0, text: "ifft~" })[0] === 2 && objectArity({ id: 930, kind: "obj", x: 0, y: 0, text: "ifft~" })[1] === 2, "ifft~ should expose real/imaginary signal ports");
assert(objectArity({ id: 931, kind: "obj", x: 0, y: 0, text: "line~" })[0] === 2 && objectArity({ id: 932, kind: "obj", x: 0, y: 0, text: "line~" })[1] === 1, "line~ should expose its main inlet, time inlet, and signal outlet");
assert(objectArity({ id: 933, kind: "obj", x: 0, y: 0, text: "vline~" })[0] === 3 && objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "vline~" })[1] === 1, "vline~ should expose its value/time/delay inlets and signal outlet");
assert(objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "samplerate~" })[0] === 1 && objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "samplerate~" })[1] === 1, "samplerate~ should expose Pd's main bang inlet and float outlet");
assert(objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "block~" })[0] === 1 && objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "block~" })[1] === 0, "block~ should expose Pd's main inlet and no outlet");
assert(objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "switch~" })[0] === 1 && objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "switch~" })[1] === 0, "switch~ should expose Pd's main inlet and no outlet");
assert(objectArity({ id: 935, kind: "obj", x: 0, y: 0, text: "snapshot~" })[0] === 1 && objectArity({ id: 936, kind: "obj", x: 0, y: 0, text: "vsnapshot~" })[0] === 1, "snapshot~ objects should accept bang on their main inlet rather than exposing a fake extra inlet");
assert(objectArity({ id: 935, kind: "obj", x: 0, y: 0, text: "threshold~" })[0] === 2 && objectArity({ id: 936, kind: "obj", x: 0, y: 0, text: "threshold~" })[1] === 2, "threshold~ should expose set inlet plus high/low bang outlets");
assert(objectArity({ id: 937, kind: "obj", x: 0, y: 0, text: "tabwrite~ sample_table" })[0] === 1 && objectArity({ id: 938, kind: "obj", x: 0, y: 0, text: "tabwrite~ sample_table" })[1] === 0, "tabwrite~ should expose only its main signal inlet and no outlet");
assert(objectArity({ id: 933, kind: "obj", x: 0, y: 0, text: "tabread~ sample_table" })[0] === 2 && objectArity({ id: 933, kind: "obj", x: 0, y: 0, text: "tabread~ sample_table" })[1] === 1, "tabread~ should expose Pd's index and onset signal inlets");
assert(objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "tabread4~ sample_table" })[0] === 2, "tabread4~ should expose index and onset signal inlets");
assert(objectArity({ id: 935, kind: "obj", x: 0, y: 0, text: "tabplay~ sample_table" })[1] === 2, "tabplay~ should expose signal and done-bang outlets");
assert(objectArity({ id: 936, kind: "obj", x: 0, y: 0, text: "soundfiler" })[1] === 2, "soundfiler should expose frame-count and soundfile-info outlets");
assert(objectArity({ id: 931, kind: "obj", x: 0, y: 0, text: "bonk~ -nsigs 4" })[0] === 4 && objectArity({ id: 932, kind: "obj", x: 0, y: 0, text: "bonk~ -nsigs 4" })[1] === 5, "bonk~ should expose one inlet/outlet per input signal plus cooked output");
assert(objectArity({ id: 933, kind: "obj", x: 0, y: 0, text: "fiddle~ 1024 3 20 2" })[1] === 7, "fiddle~ should expose note/attack/pitch/env/peak outlets from constructor arguments");
assert(objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "sigmund~ pitch env peaks tracks" })[1] === 4, "sigmund~ should expose one outlet per requested analysis selector");
function topLevelPortCountsFromPdFile(name){
 const source = require("fs").readFileSync(${JSON.stringify(helpPatchDir)} + "/" + name + ".pd", "utf8");
 let depth = 0, inlets = 0, outlets = 0;
 source.split(/\\r?\\n/).forEach(raw => {
  const line = raw.trim();
  if(line.startsWith("#N canvas ")){ depth++; return; }
  if(line.startsWith("#X restore ")){ depth = Math.max(0, depth - 1); return; }
  if(depth !== 1 || !line.startsWith("#X obj "))return;
  const atoms = splitPdAtoms(stripSemi(line.slice(7)));
  const objectName = atoms[2] || "";
  if(objectName === "inlet" || objectName === "inlet~")inlets++;
  else if(objectName === "outlet" || objectName === "outlet~")outlets++;
 });
 return [inlets, outlets];
}
["rev1~","rev2~","rev3~","hilbert~","complex-mod~","output~"].forEach((name, offset) => {
 const actual = topLevelPortCountsFromPdFile(name);
 const shown = objectArity({ id: 985 + offset, kind: "obj", x: 0, y: 0, text: name });
 assert(shown[0] === actual[0] && shown[1] === actual[1], "bundled " + name + " editor ports " + shown.join("/") + " should match Pd file " + actual.join("/"));
});
assert(objectArity({ id: 927, kind: "obj", x: 0, y: 0, text: "openpanel" })[0] === 1 && objectArity({ id: 928, kind: "obj", x: 0, y: 0, text: "savepanel" })[1] === 1, "panel objects should expose bang inlet and path outlet");
assert(objectArity({ id: 929, kind: "obj", x: 0, y: 0, text: "makefilename file.%d" })[0] === 1 && objectArity({ id: 930, kind: "obj", x: 0, y: 0, text: "makefilename file.%d" })[1] === 1, "makefilename should expose one value inlet and one symbol outlet");
assert(objectArity({ id: 943, kind: "obj", x: 0, y: 0, text: "readsf~" })[1] === 2, "bare readsf~ should expose one signal outlet plus done outlet");
assert(objectArity({ id: 944, kind: "obj", x: 0, y: 0, text: "readsf~ 4" })[1] === 5, "readsf~ channel argument should expose one signal outlet per channel plus done outlet");
assert(objectArity({ id: 945, kind: "obj", x: 0, y: 0, text: "readsf~ -m 4" })[1] === 2, "readsf~ -m should expose one multichannel signal outlet plus done outlet");
assert(objectArity({ id: 946, kind: "obj", x: 0, y: 0, text: "writesf~ 4" })[0] === 4, "writesf~ channel argument should expose one signal inlet per channel");
assert(objectArity({ id: 966, kind: "obj", x: 0, y: 0, text: "snake_in~ 6" })[0] === 6 && objectArity({ id: 967, kind: "obj", x: 0, y: 0, text: "snake_in~ 6" })[1] === 1, "snake_in~ should expose one inlet per channel and one multichannel outlet");
assert(objectArity({ id: 968, kind: "obj", x: 0, y: 0, text: "snake_out~ 5" })[0] === 1 && objectArity({ id: 969, kind: "obj", x: 0, y: 0, text: "snake_out~ 5" })[1] === 5, "snake_out~ should expose one multichannel inlet and one outlet per channel");
assert(objectArity({ id: 970, kind: "obj", x: 0, y: 0, text: "snake~ 4" })[0] === 4 && objectArity({ id: 971, kind: "obj", x: 0, y: 0, text: "snake~ in 4" })[0] === 4, "snake~ should default to multichannel input packing");
assert(objectArity({ id: 972, kind: "obj", x: 0, y: 0, text: "snake~ out 3" })[0] === 1 && objectArity({ id: 973, kind: "obj", x: 0, y: 0, text: "snake~ out 3" })[1] === 3, "snake~ out should expose multichannel unpacking ports");
assert(objectArity({ id: 947, kind: "obj", x: 0, y: 0, text: "midiin" })[1] === 2 && objectArity({ id: 948, kind: "obj", x: 0, y: 0, text: "sysexin" })[1] === 2, "midiin/sysexin should expose byte and port outlets");
assert(objectArity({ id: 949, kind: "obj", x: 0, y: 0, text: "midiout" })[0] === 2, "midiout should expose data and port inlets");
assert(objectArity({ id: 949, kind: "obj", x: 0, y: 0, text: "noteout 3" })[0] === 3 && objectArity({ id: 950, kind: "obj", x: 0, y: 0, text: "ctlout 7 3" })[0] === 3, "noteout/ctlout should keep Pd's cold velocity/control and channel inlets even with creation arguments");
assert(objectArity({ id: 951, kind: "obj", x: 0, y: 0, text: "pgmout 3" })[0] === 2 && objectArity({ id: 952, kind: "obj", x: 0, y: 0, text: "bendout 3" })[0] === 2 && objectArity({ id: 953, kind: "obj", x: 0, y: 0, text: "touchout 3" })[0] === 2, "single-value MIDI outputs should keep Pd's channel inlet even with creation arguments");
assert(objectArity({ id: 954, kind: "obj", x: 0, y: 0, text: "polytouchout 3" })[0] === 3, "polytouchout should expose value, pitch, and channel inlets");
assert(objectArity({ id: 955, kind: "obj", x: 0, y: 0, text: "makenote 90 250" })[0] === 3 && objectArity({ id: 956, kind: "obj", x: 0, y: 0, text: "stripnote" })[0] === 2, "makenote/stripnote should expose Pd's velocity and duration/velocity cold inlets");
assert(objectArity({ id: 950, kind: "obj", x: 0, y: 0, text: "polytouchin" })[1] === 3 && objectArity({ id: 951, kind: "obj", x: 0, y: 0, text: "polytouchin 2" })[1] === 2, "polytouchin should drop channel outlet when channel is specified");
assert(objectArity({ id: 959, kind: "obj", x: 0, y: 0, text: "notein 0" })[1] === 3, "notein 0 should keep Pd's all-channel outlet");
assert(objectArity({ id: 960, kind: "obj", x: 0, y: 0, text: "ctlin 7 0" })[1] === 2 && objectArity({ id: 961, kind: "obj", x: 0, y: 0, text: "ctlin 7 2" })[1] === 1, "ctlin should only drop channel outlet for nonzero channel arguments");
assert(objectArity({ id: 962, kind: "obj", x: 0, y: 0, text: "pgmin 0" })[1] === 2 && objectArity({ id: 963, kind: "obj", x: 0, y: 0, text: "bendin 0" })[1] === 2 && objectArity({ id: 964, kind: "obj", x: 0, y: 0, text: "touchin 0" })[1] === 2, "zero-channel MIDI inputs should keep channel outlets");
assert(objectArity({ id: 965, kind: "obj", x: 0, y: 0, text: "polytouchin 0" })[1] === 3, "polytouchin 0 should keep Pd's all-channel outlet");
assert(objectArity({ id: 958, kind: "obj", x: 0, y: 0, text: "midirealtimein" })[1] === 2, "midirealtimein should expose byte and port outlets");
assert(objectArity({ id: 952, kind: "obj", x: 0, y: 0, text: "oscparse" })[1] === 2, "oscparse should expose parsed list and address-size outlets");
assert(objectArity({ id: 952, kind: "obj", x: 0, y: 0, text: "oscformat -f is /note/on" })[0] === 1 && objectArity({ id: 953, kind: "obj", x: 0, y: 0, text: "oscformat -f is /note/on" })[1] === 1, "oscformat should expose Pd's message inlet and byte-list outlet");
assert(objectArity({ id: 953, kind: "obj", x: 0, y: 0, text: "fudiformat -u" })[0] === 1 && objectArity({ id: 954, kind: "obj", x: 0, y: 0, text: "fudiparse" })[1] === 1, "FUDI format/parse should expose one message inlet and one message outlet");
assert(objectArity({ id: 953, kind: "obj", x: 0, y: 0, text: "netsend" })[1] === 2, "netsend should expose connect-state and received-message outlets");
assert(objectArity({ id: 954, kind: "obj", x: 0, y: 0, text: "netreceive 3000" })[1] === 2, "TCP netreceive should expose message and connection-count outlets");
assert(objectArity({ id: 955, kind: "obj", x: 0, y: 0, text: "netreceive -u 3000" })[1] === 1, "UDP netreceive should omit the TCP connection-count outlet");
assert(objectArity({ id: 956, kind: "obj", x: 0, y: 0, text: "netreceive -u -f 3000" })[1] === 2, "netreceive -f should expose the sender-address outlet");
assert(objectArity({ id: 957, kind: "obj", x: 0, y: 0, text: "netreceive 3000 0 old" })[1] === 1, "old-style TCP netreceive should expose only the connection-count outlet");
assert(objectArity({ id: 929, kind: "obj", x: 0, y: 0, text: "file define notes" })[0] === 0 && objectArity({ id: 939, kind: "obj", x: 0, y: 0, text: "file define notes" })[1] === 0, "file define should behave as storage with no patch ports");
assert(objectArity({ id: 934, kind: "obj", x: 0, y: 0, text: "file" })[0] === 1 && objectArity({ id: 935, kind: "obj", x: 0, y: 0, text: "file handle $0-log" })[0] === 1 && objectArity({ id: 936, kind: "obj", x: 0, y: 0, text: "file handle $0-log" })[1] === 2, "file/file handle should expose Pd's main inlet and data/info outlets");
["file which","file patchpath","file glob","file stat","file size","file isfile","file isdirectory","file mkdir","file delete","file copy","file move","file cwd","file split","file join","file splitext","file splitname","file isabsolute","file normalize"].forEach((name, offset) => {
 assert(objectArity({ id: 980 + offset, kind: "obj", x: 0, y: 0, text: name })[0] === 1 && objectArity({ id: 1000 + offset, kind: "obj", x: 0, y: 0, text: name })[1] === 2, name + " should expose Pd's main inlet and data/info outlets");
});
assert(objectArity({ id: 936, kind: "obj", x: 0, y: 0, text: "savestate" })[0] === 1 && objectArity({ id: 937, kind: "obj", x: 0, y: 0, text: "savestate" })[1] === 2, "savestate should expose state input plus state/bang outlets");
assert(objectArity({ id: 930, kind: "obj", x: 0, y: 0, text: "declare -path ./lib" })[0] === 0 && objectArity({ id: 931, kind: "obj", x: 0, y: 0, text: "namecanvas $0-this" })[1] === 0, "metadata objects should not expose patch ports");
assert(objectArity({ id: 932, kind: "obj", x: 0, y: 0, text: "scalar define point" })[0] === 0 && objectArity({ id: 933, kind: "obj", x: 0, y: 0, text: "template point" })[1] === 0, "data declaration objects should not expose patch ports");
assert(objectArity({ id: 964, kind: "obj", x: 0, y: 0, text: "trace" })[0] === 2 && objectArity({ id: 965, kind: "obj", x: 0, y: 0, text: "trace" })[1] === 1, "trace should expose Pd's message inlet, float inlet, and passthrough outlet");
["backtracer","objectmaker","canvasmaker","canvas","bindlist","clone-inlet","clone-outlet","exprproxy","gfxstub","guiconnect","libpd_receive","messresponder"].forEach((name, offset) => {
 assert(objectArity({ id: 966 + offset, kind: "obj", x: 0, y: 0, text: name })[0] === 0 && objectArity({ id: 986 + offset, kind: "obj", x: 0, y: 0, text: name })[1] === 0, name + " should be represented as a non-patchable Pd implementation object");
});
assert(objectArity({ id: 906, kind: "obj", x: 0, y: 0, text: "array max sample_table" })[0] === 3 && objectArity({ id: 906, kind: "obj", x: 0, y: 0, text: "array max sample_table" })[1] === 2, "array max should expose Pd's source/onset/count inlets and value/index outlets");
assert(objectArity({ id: 907, kind: "obj", x: 0, y: 0, text: "array min sample_table" })[0] === 3 && objectArity({ id: 907, kind: "obj", x: 0, y: 0, text: "array min sample_table" })[1] === 2, "array min should expose Pd's source/onset/count inlets and value/index outlets");
assert(objectArity({ id: 908, kind: "obj", x: 0, y: 0, text: "array random sample_table" })[0] === 3 && objectArity({ id: 908, kind: "obj", x: 0, y: 0, text: "array random sample_table" })[1] === 1, "array random should expose range/source inlets and one outlet");
assert(objectArity({ id: 909, kind: "obj", x: 0, y: 0, text: "array sum sample_table" })[0] === 3 && objectArity({ id: 909, kind: "obj", x: 0, y: 0, text: "array sum sample_table" })[1] === 1, "array sum should expose Pd's onset/count/source inlets and summed-value outlet");
assert(objectArity({ id: 938, kind: "obj", x: 0, y: 0, text: "array get sample_table" })[0] === 3 && objectArity({ id: 939, kind: "obj", x: 0, y: 0, text: "array set sample_table" })[0] === 3, "array get/set should expose Pd's onset/count/source inlets");
assert(objectArity({ id: 941, kind: "obj", x: 0, y: 0, text: "array quantile sample_table" })[0] === 4 && objectArity({ id: 942, kind: "obj", x: 0, y: 0, text: "array size sample_table" })[0] === 2, "array quantile/size should expose Pd's extra parameter/source inlets");
assert(objectArity({ id: 943, kind: "obj", x: 0, y: 0, text: "array define sample_table 8" })[0] === 1 && objectArity({ id: 944, kind: "obj", x: 0, y: 0, text: "array define sample_table 8" })[1] === 1, "typed array define should expose Pd's message inlet and pointer outlet");
assert(objectArity({ id: 909, kind: "obj", x: 0, y: 0, text: "array define sample_table 8", sourceKind: "array" })[1] === 0, "array storage objects should not expose edit ports");
assert(objectArity({ id: 910, kind: "obj", x: 0, y: 0, text: "table sample_table 64" })[0] === 0 && objectArity({ id: 911, kind: "obj", x: 0, y: 0, text: "array visual_array 16" })[1] === 0, "Pd table/visual array storage should not expose generic patch ports");
assert(objectArity({ id: 912, kind: "obj", x: 0, y: 0, text: "pd subpatch" })[0] === 0 && objectArity({ id: 913, kind: "obj", x: 0, y: 0, text: "page subpatch" })[1] === 0, "Pd subcanvas creators should not expose generic patch ports");
assert(objectArity({ id: 914, kind: "obj", x: 0, y: 0, text: "message" })[0] === 0 && objectArity({ id: 915, kind: "obj", x: 0, y: 0, text: "message" })[1] === 0, "literal Pd message implementation class should not expose generic patch ports");
const explicitlyDynamicObjects = new Set(["trigger","t","select","sel","route","pack","unpack","poly","clone","list append","list prepend","list split","text get","text search","text sequence","swap","fswap","expr","expr~","fexpr~","adc~","dac~","readsf~","writesf~","snake~","snake_in~","snake_out~","notein","ctlin","pgmin","bendin","touchin","polytouchin","netreceive","netsend","pd~","inlet","inlet~","outlet","outlet~","send","s","send~","s~","throw~","print","receive","r","receive~","r~","catch~"]);
const acceptableGenericAdvertisedObjects = new Set();
const advertisedNames = pdObjectGroups.flatMap(([, names]) => names);
const arityNames = new Set(Object.keys(arity));
const duplicateAdvertisedNames = advertisedNames.filter((name, index) => advertisedNames.indexOf(name) !== index);
assert(duplicateAdvertisedNames.length === 0, "Pd object palette should not contain duplicate names: " + duplicateAdvertisedNames.join(", "));
const advertisedObjectsFallingThroughGeneric = advertisedNames.filter(name => !arityNames.has(name) && !explicitlyDynamicObjects.has(name) && !guiObjects.has(name) && !dataStructureObjects.has(name));
assert(advertisedObjectsFallingThroughGeneric.every(name => acceptableGenericAdvertisedObjects.has(name)), "advertised Pd object has no explicit editor handling: " + advertisedObjectsFallingThroughGeneric.filter(name => !acceptableGenericAdvertisedObjects.has(name)).join(", "));
assert([...acceptableGenericAdvertisedObjects].every(name => advertisedObjectsFallingThroughGeneric.includes(name)), "generic Pd object allowlist is stale: " + [...acceptableGenericAdvertisedObjects].filter(name => !advertisedObjectsFallingThroughGeneric.includes(name)).join(", "));
function registeredPdClassNamesFromSource() {
 const roots = ["third_party/pure-data/src", "third_party/pure-data/extra"];
 const files = [];
 const walk = dir => {
  if (!require("fs").existsSync(dir)) return;
  require("fs").readdirSync(dir, { withFileTypes: true }).forEach(entry => {
   const path = dir + "/" + entry.name;
   if (entry.isDirectory()) walk(path);
   else if (/\.c$/.test(entry.name)) files.push(path);
  });
 };
 roots.forEach(walk);
 const names = new Set();
 files.forEach(path => {
  const source = require("fs").readFileSync(path, "utf8");
  const patterns = [
    new RegExp("(?:class_new|class_addcreator)\\\\s*\\\\([^;]*?gensym\\\\s*\\\\(\\\\s*\\\"([^\\\"]+)\\\"\\\\s*\\\\)", "gs"),
    new RegExp("class_new\\\\s*\\\\(\\\\s*\\\"([^\\\"]+)\\\"", "gs"),
    new RegExp("class_addcreator\\\\s*\\\\([^,]+,\\\\s*\\\"([^\\\"]+)\\\"", "gs")
  ];
  patterns.forEach(re => {
    let match;
    while ((match = re.exec(source))) names.add(match[1]);
  });
 });
 return names;
}
const pdSelectorPseudoClasses = new Set(["signal", "pointer", "float", "symbol", "list", "bang", "anything", "blob", "A_DEFFLOAT", "A_GIMME"]);
const missingRegisteredPdClasses = [...registeredPdClassNamesFromSource()].filter(name => !pdSelectorPseudoClasses.has(name) && !knownObjects.has(name)).sort();
assert(missingRegisteredPdClasses.length === 0, "editor is missing Pd-registered classes: " + missingRegisteredPdClasses.join(", "));
assert(rectIntersects({ x: 10, y: 10, w: 20, h: 20 }, { x: 28, y: 28, w: 80, h: 30 }), "marquee selection should include touched Pd boxes");
assert(!rectIntersects({ x: 10, y: 10, w: 20, h: 20 }, { x: 40, y: 40, w: 80, h: 30 }), "marquee selection should ignore untouched Pd boxes");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 pack f f f f;\\n#X obj 40 110 dac~;\\n",false);
view = { x: 0, y: 0, w: 900, h: 620 };
render();
const visiblePorts = svg.querySelectorAll(".port");
const portHitRects = svg.querySelectorAll(".portHit");
assert(visiblePorts.length >= 6 && portHitRects.length >= 6, "Pd ports should render visible tabs and separate hit targets");
assert(visiblePorts.every(port => Number(port.getAttribute("width")) * screenScaleX() <= 4.1 && Number(port.getAttribute("height")) * screenScaleY() <= 1.6), "visible Pd ports should stay as small screen-space tabs");
assert(portHitRects.every(port => Number(port.getAttribute("width")) * screenScaleX() >= 15.9 && Number(port.getAttribute("height")) * screenScaleY() >= 11.9), "Pd port hit targets should remain comfortably draggable");
assert([...visiblePorts].every(port => port.getAttribute("vector-effect") === "non-scaling-stroke"), "visible Pd port outlines should not scale into oversized handles");
assert([...portHitRects].every(port => String(port.getAttribute("style")).includes("stroke-width:0")), "Pd port hit targets should stay invisible even on selected objects");
selectedSet = new Set([0]);
render();
const selectedVisiblePorts = [...svg.querySelectorAll(".box.selected .port")];
const selectedHitPorts = [...svg.querySelectorAll(".box.selected .portHit")];
assert(selectedVisiblePorts.every(port => Number(port.getAttribute("width")) * screenScaleX() <= 4.1 && String(port.getAttribute("style")).includes("stroke-width:.45")), "selected Pd ports should remain tiny Pd-style tabs");
assert(selectedHitPorts.every(port => String(port.getAttribute("style")).includes("stroke-width:0")), "selected Pd port hit targets should remain visually hidden");
const portAt100 = Number(visiblePorts[0].getAttribute("width")) * screenScaleX();
view = { x: 0, y: 0, w: 450, h: 310 };
render();
const portAt200 = Number(svg.querySelectorAll(".port")[0].getAttribute("width")) * screenScaleX();
assert(Math.abs(portAt100 - portAt200) < 0.001, "visible Pd ports should not become visually larger when zoomed in");
const zoomedPort = portPoint(objects[0], true, 0);
assert(portNearPoint({ x: zoomedPort.x + canvasUnitsX(9), y: zoomedPort.y }, "inlet", -1, 10).index === 0, "nearby Pd port targeting should use screen-space distance when zoomed in");
assert(!portNearPoint({ x: zoomedPort.x + canvasUnitsX(11), y: zoomedPort.y }, "inlet", -1, 10), "Pd port targeting should reject points outside the screen-space tolerance");
const edgeTarget = {
  closest(selector) {
    if (selector === ".box") return { getAttribute: key => key === "data-index" ? "0" : "" };
    return null;
  }
};
assert(startPortAt({ target: edgeTarget }, "inlet", { x: zoomedPort.x + canvasUnitsX(5), y: zoomedPort.y }).index === 0, "near-edge clicks inside a Pd box should still start a cable from the port");
parsePatch("#N canvas 20 30 620 360 10;\\n"
 + "#X obj 40 40 struct no-port-template float x float y;\\n"
 + "#X obj 40 80 drawpolygon 222 1 0 0 x y;\\n"
 + "#X scalar no-port-template 8 12;\\n"
 + "#X obj 300 80 osc~ 220;\\n",false);
render();
const dataPortCount = svg.querySelectorAll(".box.datastruct .port,.box.datastruct .portHit,.box.scalar .port,.box.scalar .portHit").length;
assert(dataPortCount === 0, "Pd data/scalar boxes should not render patch ports");
assert(!portNearPoint({ x: objects[0].x + 20, y: objects[0].y }, "inlet", -1, 80), "Pd data declaration should not be an invisible inlet target");
assert(!portNearPoint({ x: objects[2].x + 20, y: objects[2].y }, "outlet", -1, 80), "Pd scalar should not be an invisible outlet target");
parsePatch("#N canvas 20 30 520 360 10;\\n"
 + "#X obj 40 40 osc~ 220;\\n"
 + "#X obj 40 120 *~;\\n"
 + "#X obj 260 120 *~;\\n"
 + "#X connect 0 0 1 0;\\n",false);
const oldConnection = connections.splice(0,1)[0];
patchCord = makePatchCordFromPort("outlet", { index: oldConnection.from, port: oldConnection.out || 0 }, portPoint(objects[oldConnection.from], false, oldConnection.out || 0), true);
patchCord.moved = true;
assert(finishPatchCordAt(portPoint(objects[2], true, 0), { target: null }), "dragging a cable to another inlet should reconnect it");
assert(connections.length === 1 && connections[0].from === 0 && connections[0].to === 2 && connections[0].inlet === 0, "reconnected patch cable did not target the new inlet");
parsePatch("#N canvas 20 30 640 360 10;\\n"
 + "#X obj 40 40 osc~ 220;\\n"
 + "#X obj 40 120 *~;\\n"
 + "#X obj 260 120 *~;\\n"
 + "#X obj 40 220 dac~;\\n"
 + "#X connect 0 0 1 0;\\n"
 + "#X connect 1 0 3 0;\\n"
 + "#X connect 2 0 3 1;\\n",false);
const orderedOutletDetach = connections.splice(1,1)[0];
patchCord = makePatchCordFromPort("outlet", { index: orderedOutletDetach.from, port: orderedOutletDetach.out || 0 }, portPoint(objects[orderedOutletDetach.from], false, orderedOutletDetach.out || 0), true, orderedOutletDetach);
patchCord.detachedIndex = 1;
patchCord.moved = true;
assert(finishPatchCordAt(portPoint(objects[3], true, 1), { target: null }), "outlet-end reconnection should finish against the new inlet");
assert(connections[0].from === 0 && connections[0].to === 1 && connections[1].from === 1 && connections[1].to === 3 && connections[1].inlet === 1 && connections[2].from === 2, "outlet-end reconnection should preserve the original connection slot");
parsePatch("#N canvas 20 30 640 360 10;\\n"
 + "#X obj 40 40 osc~ 220;\\n"
 + "#X obj 260 40 noise~;\\n"
 + "#X obj 40 120 *~;\\n"
 + "#X obj 40 220 dac~;\\n"
 + "#X connect 0 0 2 0;\\n"
 + "#X connect 2 0 3 0;\\n"
 + "#X connect 1 0 3 1;\\n",false);
const orderedInletDetach = connections.splice(1,1)[0];
patchCord = makePatchCordFromPort("inlet", { index: orderedInletDetach.to, port: orderedInletDetach.inlet || 0 }, portPoint(objects[orderedInletDetach.to], true, orderedInletDetach.inlet || 0), true, orderedInletDetach);
patchCord.detachedIndex = 1;
patchCord.fromPoint = portPoint(objects[0], false, 0);
patchCord.moved = true;
assert(finishPatchCordAt(portPoint(objects[0], false, 0), { target: null }), "inlet-end reconnection should finish against the new outlet");
assert(connections[0].from === 0 && connections[0].to === 2 && connections[1].from === 0 && connections[1].to === 3 && connections[1].inlet === 0 && connections[2].from === 1, "inlet-end reconnection should preserve the original connection slot");
parsePatch("#N canvas 20 30 520 360 10;\\n"
 + "#X obj 40 40 osc~ 220;\\n"
 + "#X obj 40 120 *~;\\n"
 + "#X obj 260 120 *~;\\n"
 + "#X connect 0 0 2 0;\\n",false);
const noOpDetach = connections.splice(0,1)[0];
patchCord = makePatchCordFromPort("outlet", { index: noOpDetach.from, port: noOpDetach.out || 0 }, portPoint(objects[noOpDetach.from], false, noOpDetach.out || 0), true, noOpDetach);
patchCord.detachedIndex = 0;
assert(!finishPatchCordAt(patchCord.start, { target: null }), "no-op cable detach should not count as a changed patch");
assert(connections.length === 1 && connections[0].from === 0 && connections[0].to === 2, "no-op cable detach did not restore the original cord");
const escapeDetach = connections.splice(0,1)[0];
patchCord = makePatchCordFromPort("outlet", { index: escapeDetach.from, port: escapeDetach.out || 0 }, portPoint(objects[escapeDetach.from], false, escapeDetach.out || 0), true, escapeDetach);
patchCord.detachedIndex = 0;
patchCord.moved = true;
backendEvents.length = 0;
let cableEscapePrevented = false;
document.dispatchEvent({ type: "keydown", key: "Escape", metaKey: false, ctrlKey: false, target: null, preventDefault(){ cableEscapePrevented = true; } });
assert(cableEscapePrevented, "Escape should handle an active Pd cable detach gesture");
assert(connections.length === 1 && connections[0].from === escapeDetach.from && connections[0].to === escapeDetach.to, "Escape should restore a detached Pd patch cable instead of deleting it");
assert(!backendEvents.some(event => event.name === "patchChanged"), "cancelling a Pd cable detach with Escape should not emit a changed patch");
const deletedConnection = connections.splice(0,1)[0];
patchCord = makePatchCordFromPort("inlet", { index: 2, port: 0 }, { x: 260, y: 120 }, true, deletedConnection);
patchCord.detachedIndex = 0;
patchCord.fromPoint = portPoint(objects[0], false, 0);
patchCord.moved = true;
assert(finishPatchCordAt({ x: 420, y: 260 }, { target: null }), "dragging a cable into empty space should complete as a deletion");
assert(connections.length === 0, "dragging a patch cable into empty space should remove the old connection");
patchCord = makePatchCordFromPort("outlet", { index: deletedConnection.from, port: deletedConnection.out || 0 }, portPoint(objects[deletedConnection.from], false, deletedConnection.out || 0), true, deletedConnection);
patchCord.detachedIndex = 0;
cancelPatchCord(true);
assert(connections.length === 1 && connections[0].to === 2, "cancelled cable drag should restore the detached cord");
parsePatch("#N canvas 20 30 640 360 10;\\n"
 + "#X obj 40 40 osc~ 220;\\n"
 + "#X obj 220 40 noise~;\\n"
 + "#X obj 40 140 *~;\\n"
 + "#X connect 0 0 2 0;\\n"
 + "#X connect 1 0 2 0;\\n",false);
assert(topmostConnectionIndexForInlet(2,0) === 1, "inlet cable grabs should target the topmost/latest cord when several enter the same inlet");
const topmostDetach = connections.splice(topmostConnectionIndexForInlet(2,0),1)[0];
assert(topmostDetach.from === 1 && connections.length === 1 && connections[0].from === 0, "detaching from a crowded inlet should remove the topmost cord and leave the lower cord in place");
patchCord = makePatchCordFromPort("outlet", { index: topmostDetach.from, port: topmostDetach.out || 0 }, portPoint(objects[topmostDetach.from], false, topmostDetach.out || 0), true, topmostDetach);
patchCord.detachedIndex = 1;
cancelPatchCord(true);
assert(connections.length === 2 && connections[0].from === 0 && connections[1].from === 1, "cancelling a crowded-inlet detach should restore the topmost cord in its original visual order");
const arrayFixture = "#N canvas 20 30 620 360 10;\\n"
 + "#X obj 20 20 array define sample_table 8;\\n"
 + "#X obj 20 70 array set sample_table;\\n"
 + "#X obj 20 120 array get sample_table;\\n"
 + "#X obj 220 70 array sum sample_table;\\n"
 + "#X obj 220 120 array quantile sample_table;\\n"
 + "#X obj 420 70 array random sample_table;\\n"
 + "#X obj 420 120 array max sample_table;\\n"
 + "#X obj 420 170 array min sample_table;\\n"
 + "#X connect 2 0 3 0;\\n"
 + "#X connect 6 0 7 0;\\n";
parsePatch(arrayFixture,false);
const arrayOut = serialize();
assert(arrayOut.includes("#X obj 20 20 array define sample_table 8;"), "array define did not round-trip");
assert(arrayOut.includes("#X obj 220 120 array quantile sample_table;"), "array quantile did not round-trip");
assert(arrayOut.includes("#X obj 420 70 array random sample_table;"), "array random did not round-trip");
assert(arrayOut.includes("#X obj 420 120 array max sample_table;"), "array max did not round-trip");
assert(arrayOut.includes("#X obj 420 170 array min sample_table;"), "array min did not round-trip");
assert(objects.every(o => isKnownObject(o)), "array object family should be known to the editor");
assert(objectArity(objects.find(o => canonicalName(o) === "array max"))[1] === 2, "imported array max should keep two outlets");
assert(objectArity(objects.find(o => canonicalName(o) === "array min"))[1] === 2, "imported array min should keep two outlets");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addObject("obj",20,20,"array define typed_table 8");
assert(objects[0] && canonicalName(objects[0]) === "array define" && !isArrayLike(objects[0]), "typed array define should remain a Pd object, not become a visual array store");
assert(serialize().includes("#X obj 20 20 array define typed_table 8;"), "typed array define object did not serialize as a Pd object");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addObject("obj",30,36,"array visual_array 16 float 3");
assert(objects[0] && objects[0].sourceKind === "array" && objects[0].arrayName === "visual_array" && objects[0].arraySize === 16, "typed visual array should become editable visual array storage");
assert(serialize().includes("#X array visual_array 16 float 3;"), "typed visual array did not serialize as #X array storage");
parsePatch("#N canvas 20 30 360 220 10;\\n#X array table\\\\ name 8 float 3;\\n#A 0 1 2 3;\\n",false);
assert(objects[0].arrayName === "table name", "escaped-space array name was not readable after import");
assert(serialize().includes("#X array table\\\\ name 8 float 3;"), "escaped-space array header did not round-trip");
parsePatch("#N canvas 20 30 520 360 10;\\n#X array graph_array 8 float 0;\\n#A 0 0 1 0 -1 0 1 0 -1;\\n#X coords 0 1 7 -1 200 140 1 0 0;\\n#X restore 220 160 graph;\\n",false);
assert(serialize().includes("#X restore 220 160 graph;"), "imported graph-array restore line did not round-trip");
assert(arrayValues(objects[0]).join(",") === "0,1,0,-1,0,1,0,-1", "graph-array data should honor indexed #A blocks and declared size");
const graphPlot = arrayPlotRect(objects[0]);
history=[];historyIndex=-1;emit();
assert(!beginArrayDraw(0,{x:graphPlot.left,y:objects[0].y+8}), "array title clicks should not paint waveform samples");
assert(beginArrayDraw(0,{x:graphPlot.left,y:graphPlot.top}), "run-mode graph-array drawing should begin on visual arrays");
assert(continueArrayDraw({x:graphPlot.right,y:graphPlot.bottom}), "run-mode graph-array drawing should continue across the waveform");
arrayDrag=null;emit();
const drawnValues=arrayValues(objects[0]);
assert(Math.abs(drawnValues[0]-1)<0.000001&&Math.abs(drawnValues[7]+1)<0.000001, "array drawing should map the graph's Pd y range to samples");
assert(drawnValues.slice(1,7).every((value,index)=>value<drawnValues[index]&&value>drawnValues[index+2]), "fast array drawing should interpolate every crossed sample");
assert((serialize().match(/#A 0 /g)||[]).length===1&&serialize().includes("#A 0 1 "), "drawn graph arrays should serialize as indexed Pd #A data");
assert(undoPatch()&&arrayValues(objects[0]).join(",")==="0,1,0,-1,0,1,0,-1", "array waveform drawing should be one undoable patch edit");
parsePatch("#N canvas 20 30 520 360 10;\\n#X array graph_array 8 float 0;\\n#A 0 0 1 0 -1 0 1 0 -1;\\n#X coords 0 1 7 -1 200 140 1 0 0;\\n#X restore 220 160 graph;\\n",false);
assert(openSourceObject(0), "graph-array storage should open in the source editor");
sourceText.value = "#X array moved_graph 8 float 0;\\n#A 0 0 1 0 -1 0 1 0 -1;\\n#X coords 0 1 7 -1 200 140 1 0 0;\\n#X restore 310 190 graph;\\n";
toggleSource(document.getElementById("source"));
assert(objects[0].x === 310 && objects[0].y === 190, "editing graph-array source should move the editor object to the Pd restore coordinates");
assert(serialize().includes("#X restore 310 190 graph;"), "edited graph-array restore coordinates did not serialize");
selectedSet = new Set([0]);
selected = 0;
assert(copySelection() && pasteSelection(), "graph array should copy/paste");
const graphArrayPasteOut = serialize();
assert((graphArrayPasteOut.match(/#X restore 310 190 graph;/g)||[]).length === 1, "original graph array restore wrapper should remain once");
assert((graphArrayPasteOut.match(/#X restore 340 220 graph;/g)||[]).length === 1, "pasted graph array should keep an offset graph restore wrapper");
assert((graphArrayPasteOut.match(/#X coords 0 1 7 -1 200 140 1 0 0;/g)||[]).length === 2, "pasted graph array should keep visual coords for the copied array");
parsePatch("#N canvas 20 30 520 360 10;\\n#X array duplicate_graph 8 float 0;\\n#A 0 0 1 0 -1 0 1 0 -1;\\n#X coords 0 1 7 -1 200 140 1 0 0;\\n#X restore 220 160 graph;\\n",false);
selectedSet = new Set([0]);
selected = 0;
assert(duplicateSelectionAtOffset(30,30), "graph array should duplicate");
const graphArrayDuplicateOut = serialize();
assert((graphArrayDuplicateOut.match(/#X restore 220 160 graph;/g)||[]).length === 1, "duplicating graph array should preserve the original wrapper once");
assert((graphArrayDuplicateOut.match(/#X restore 250 190 graph;/g)||[]).length === 1, "duplicated graph array should keep an offset graph restore wrapper");
assert((graphArrayDuplicateOut.match(/#X coords 0 1 7 -1 200 140 1 0 0;/g)||[]).length === 2, "duplicated graph array should keep visual coords for both arrays");
const tableFixture = "#N canvas 20 30 420 240 10;\\n"
 + "#X obj 40 50 table sample_table 64, f 21;\\n";
parsePatch(tableFixture,false);
const tableObject = objects.find(o => isArrayLike(o) && o.sourceKind === "table");
assert(tableObject && tableObject.arrayName === "sample_table" && tableObject.arraySize === 64, "table object was not recognized as editable storage");
assert(!beginArrayDraw(objects.indexOf(tableObject),{x:tableObject.x+12,y:tableObject.y+34}), "hidden Pd table storage should not behave like a drawable graph array");
assert(serialize().includes("#X obj 40 50 table sample_table 64, f 21;"), "table width hint did not round-trip");
assert(openSourceObject(objects.indexOf(tableObject)), "table storage should open in the source editor");
sourceText.value = "#X obj 40 50 table edited_table 128, f 29;\\n#X coords 0 1 127 -1 220 90 1 0 0;\\n";
toggleSource(document.getElementById("source"));
const tableOut = serialize();
assert(tableOut.includes("#X obj 40 50 table edited_table 128, f 29;"), "edited table header/width hint did not serialize as a table object");
assert(tableOut.includes("#X coords 0 1 127 -1 220 90 1 0 0;"), "edited table trailing coords did not serialize");
assert(!tableOut.includes("#X array edited_table"), "table source edit should not convert into a visual #X array");
assert(openSourceObject(objects.indexOf(tableObject)), "edited table storage should reopen in the source editor");
sourceText.value = "#X obj 40 50 table edited_table 128;\\n#X coords 0 1 127 -1 220 90 1 0 0;\\n";
toggleSource(document.getElementById("source"));
assert(serialize().includes("#X obj 40 50 table edited_table 128;") && !serialize().includes("table edited_table 128, f"), "removing a table width hint in source should remove it from serialization");
history = [];
historyIndex = -1;
parsePatch(tableFixture,false);
emit();
const undoTableObject = objects.find(o => isArrayLike(o) && o.sourceKind === "table");
assert(openSourceObject(objects.indexOf(undoTableObject)), "table storage should open before source undo regression");
sourceText.value = "#X obj 40 50 table undo_table 256;\\n#X coords 0 1 255 -1 240 100 1 0 0;\\n";
updateSourceObjectFromText(objects[editingSourceObject], sourceText.value);
emit();
assert(serialize().includes("#X obj 40 50 table undo_table 256;"), "table source edit did not enter history");
assert(undoPatch(), "undo while editing table source should succeed");
assert(sourceVisible && editingSourceObject >= 0, "undo while editing object source should keep the source pane open");
assert(sourceText.value.includes("table sample_table 64") && serialize().includes("#X obj 40 50 table sample_table 64, f 21;"), "undo while editing object source should restore both source pane text and object model");
assert(redoPatch(), "redo while editing table source should succeed");
assert(sourceText.value.includes("table undo_table 256") && serialize().includes("#X obj 40 50 table undo_table 256;"), "redo while editing object source should restore both source pane text and object model");
closeSourceEditor(false);
history = [];
historyIndex = -1;
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 50 osc~ 220;\\n",false);
emit();
toggleSource(document.getElementById("source"));
sourceText.value = "#N canvas 20 30 360 220 10;\\n#X obj 40 50 phasor~ 330;\\n";
emit();
assert(rootPatchText().includes("#X obj 40 50 phasor~ 330;"), "global source edit should enter history using the source pane text");
assert(undoPatch(), "undo while editing global source should succeed");
assert(sourceVisible && editingSourceObject < 0, "undo while editing global source should keep the source pane open");
assert(sourceText.value.includes("osc~ 220") && serialize().includes("#X obj 40 50 osc~ 220;"), "undo while editing global source should restore both source pane text and canvas objects");
assert(redoPatch(), "redo while editing global source should succeed");
assert(sourceText.value.includes("phasor~ 330") && serialize().includes("#X obj 40 50 phasor~ 330;"), "redo while editing global source should restore both source pane text and canvas objects");
closeSourceEditor(false);
parsePatch(tableFixture,false);
history = [];
historyIndex = -1;
emit();
const cancelTableObject = objects.find(o => isArrayLike(o) && o.sourceKind === "table");
assert(openSourceObject(objects.indexOf(cancelTableObject)), "table storage should open before source cancel regression");
sourceText.value = "#X obj 40 50 table cancelled_table 512;\\n#X coords 0 1 511 -1 260 110 1 0 0;\\n";
updateSourceObjectFromText(objects[editingSourceObject], sourceText.value);
emit();
assert(serialize().includes("cancelled_table 512"), "live object source edit should update the object model before cancel");
assert(history.length === 2 && historyIndex === 1, "live object source edit should enter history before cancel");
assert(!closeSourceEditor(false), "cancelled object source close should report no applied source");
assert(!sourceVisible && editingSourceObject < 0, "cancelled object source close should hide the source pane");
assert(serialize().includes("#X obj 40 50 table sample_table 64, f 21;") && !serialize().includes("cancelled_table"), "cancelled object source close should restore the original object source");
assert(history.length === 1 && historyIndex === 0 && !redoPatch(), "cancelled object source edits should be removed from redo history");
parsePatch(tableFixture,false);
const escapeTableObject = objects.find(o => isArrayLike(o) && o.sourceKind === "table");
assert(openSourceObject(objects.indexOf(escapeTableObject)), "table storage should open before source Escape regression");
sourceText.value = "#X obj 40 50 table escaped_table 1024;\\n#X coords 0 1 1023 -1 280 120 1 0 0;\\n";
updateSourceObjectFromText(objects[editingSourceObject], sourceText.value);
backendEvents.length = 0;
emit();
assert(backendEvents.some(event => event.name === "patchChanged" && String(event.payload.patch || "").includes("escaped_table")), "live source edit should publish its temporary patch before Escape");
backendEvents.length = 0;
let escapePrevented = false;
sourceText.dispatchEvent({ type: "keydown", key: "Escape", preventDefault(){ escapePrevented = true; } });
assert(escapePrevented && !sourceVisible, "Escape in object source pane should cancel and close the pane");
assert(serialize().includes("#X obj 40 50 table sample_table 64, f 21;") && !serialize().includes("escaped_table"), "Escape in object source pane should restore the original object source");
const escapeCancelPatch = backendEvents.filter(event => event.name === "patchChanged").at(-1)?.payload?.patch || "";
assert(escapeCancelPatch.includes("#X obj 40 50 table sample_table 64, f 21;") && !escapeCancelPatch.includes("escaped_table"), "Escape source cancel should publish the restored patch to the backend");
parsePatch("#N canvas 20 30 520 320 10;\\n"
 + "#X obj 40 50 table first_table 64, f 21;\\n"
 + "#X obj 240 50 text define -k notes, f 24;\\n"
 + "#A set old line;\\n",false);
const switchTable = objects.find(o => isArrayLike(o) && o.sourceKind === "table");
const switchTextStore = objects.find(o => isTextStore(o));
assert(openSourceObject(objects.indexOf(switchTable)), "table storage should open before source-switch regression");
sourceText.value = "#X obj 40 50 table committed_table 128, f 29;\\n#X coords 0 1 127 -1 220 90 1 0 0;\\n";
assert(openSourceObject(objects.indexOf(switchTextStore)), "opening a second source object should commit and switch source panes");
assert(sourceVisible && editingSourceObject === objects.indexOf(switchTextStore), "source switch should leave the second object open in the source pane");
assert(serialize().includes("#X obj 40 50 table committed_table 128, f 29;"), "switching source objects should commit the previous source pane text");
assert(sourceText.value.includes("text define -k notes"), "source switch should show the newly opened source object");
const canvasMetadataFixture = "#N canvas 64 72 760 520 12;\\n"
 + "#X obj 40 40 inlet;\\n"
 + "#X obj 40 92 outlet;\\n"
 + "#X graph graph1 0 -1 16 1 300 200 420 320;\\n"
 + "#X obj 180 40 table meta_table 32;\\n"
 + "#X connect 0 0 1 0;\\n"
 + "#X coords 0 -1 1 1 260 140 2 10 10;\\n"
 + "#X restore 480 360 graph;\\n";
parsePatch(canvasMetadataFixture,false);
const metadataOut = serialize();
assert(metadataOut.startsWith("#N canvas 64 72 760 520 12;"), "canvas header metadata did not round-trip");
assert(metadataOut.includes("#X graph graph1 0 -1 16 1 300 200 420 320;"), "root graph metadata line did not round-trip");
assert(metadataOut.includes("#X obj 180 40 table meta_table 32;"), "table inside metadata fixture did not round-trip");
assert(metadataOut.includes("#X connect 0 0 1 0;"), "connection inside metadata fixture did not round-trip");
assert(metadataOut.includes("#X coords 0 -1 1 1 260 140 2 10 10;"), "root coords metadata line did not round-trip after connections");
assert(metadataOut.includes("#X restore 480 360 graph;"), "root restore metadata line did not round-trip after connections");
assert(metadataOut.indexOf("#X graph") < metadataOut.indexOf("#X connect"), "pre-connection graph metadata should remain before connections");
assert(metadataOut.indexOf("#X coords") > metadataOut.indexOf("#X connect"), "post-connection coords metadata should remain after connections");
const mixedFixture = "#N canvas 90 120 980 680 10;\\n"
 + "#X declare -path ./abstractions -lib bob~;\\n"
 + "#X obj 36 34 namecanvas \\\\$0-main;\\n"
 + "#X obj 36 78 text define -k \\\\$0-seq, f 24;\\n"
 + "#A set step 0 bang \\\\; step 1 0.25 \\\\; step 2 symbol done;\\n"
 + "#X array \\\\$0-wave 16 float 3;\\n"
 + "#A 0 0 0.1 0.2 0.4 0.2 0.1 0 -0.1 -0.2 -0.4 -0.2 -0.1 0 0.05 0.1 0;\\n"
 + "#X coords 0 1 16 -1 180 90 1 0 0;\\n"
 + "#X obj 36 144 my_canvas 15 140 58 recvCanvas sendCanvas title 8 8 0 12 #eeeeee #111111 #333333;\\n"
 + "#X listbox 36 224 18 0 0 0 phrase phraseIn phraseOut 10;\\n"
 + "#X obj 36 284 clone voice 4 \\\\$0-voice;\\n"
 + "#N canvas 200 180 420 310 routing \\\\$0 0;\\n"
 + "#X obj 30 30 inlet;\\n"
 + "#X obj 30 76 route bang float list;\\n"
 + "#X obj 30 128 s \\\\$0-routed;\\n"
 + "#X text 30 182 inner \\\\$0 note;\\n"
 + "#X coords 0 -1 1 1 220 120 1 20 20;\\n"
 + "#X restore 310 136 pd routing \\\\$0;\\n"
 + "#X obj 310 302 r \\\\$0-routed;\\n"
 + "#X obj 520 302 array get \\\\$0-wave;\\n"
 + "#X obj 520 348 text get \\\\$0-seq;\\n"
 + "#X connect 5 0 6 0;\\n"
 + "#X connect 6 0 7 0;\\n"
 + "#X connect 8 0 9 0;\\n";
parsePatch(mixedFixture,false);
const mixedOut = serialize();
assert(mixedOut.includes("#X declare -path ./abstractions -lib bob~;"), "declare line did not round-trip in a mixed patch");
assert(declaredLibrariesFromPatch("#X declare -stdlib bob~ -lib choice;").join(",") === "bob~,choice", "root declare should parse -stdlib and -lib libraries");
assert(declaredLibrariesFromPatch("#X obj 20 20 declare -stdlib sigmund~ -lib loop~;").join(",") === "sigmund~,loop~", "object declare should parse -stdlib and -lib libraries");
assert(declaredLibrariesFromPatch("#X obj 20 20 declare -stdlib sigmund~ -lib loop~, f 32;").join(",") === "sigmund~,loop~", "formatted object declare should ignore Pd width hints");
assert(declaredLibrariesFromPatch("#X obj 20 20 symbol declare -lib fake_lib;").length === 0, "ordinary Pd objects containing the word declare should not be parsed as declarations");
assert(["rev1~","rev2~","rev3~","hilbert~","complex-mod~","output~"].every(name => bundledPdLibraries.has(name)), "editor unsupported-library status should treat shipped Pd abstraction extras as bundled");
assert(declaredLibrariesFromPatch("#X declare -stdlib rev3~ -lib hilbert~ -lib definitely_missing_otherware_lib;").filter(name => !bundledPdLibraries.has(name)).join(",") === "definitely_missing_otherware_lib", "editor unsupported-library status should only flag non-bundled declared Pd libs");
assert(mixedOut.includes("#X obj 36 34 namecanvas \\\\$0-main;"), "namecanvas $0 line did not round-trip in a mixed patch");
assert(mixedOut.includes("#X obj 36 78 text define -k \\\\$0-seq, f 24;"), "text define header width hint did not round-trip in a mixed patch");
assert(mixedOut.includes("#A set step 0 bang \\\\; step 1 0.25 \\\\; step 2 symbol done;"), "text define #A data did not round-trip in a mixed patch");
assert(mixedOut.includes("#X array \\\\$0-wave 16 float 3;"), "array header did not round-trip in a mixed patch");
assert(mixedOut.includes("#A 0 0 0.1 0.2 0.4 0.2 0.1 0 -0.1 -0.2 -0.4 -0.2 -0.1 0 0.05 0.1 0;"), "array #A data did not round-trip in a mixed patch");
assert(mixedOut.includes("#X coords 0 1 16 -1 180 90 1 0 0;"), "array coords did not round-trip in a mixed patch");
assert(mixedOut.includes("#X obj 36 144 my_canvas 15 140 58 recvCanvas sendCanvas title 8 8 0 12 #eeeeee #111111 #333333;"), "GUI alias did not round-trip in a mixed patch");
assert(mixedOut.includes("#X listbox 36 224 18 0 0 0 phrase phraseIn phraseOut 10;"), "listbox did not round-trip in a mixed patch");
assert(mixedOut.includes("#X obj 36 284 clone voice 4 \\\\$0-voice;"), "clone object dollar argument did not round-trip in a mixed patch");
assert(mixedOut.includes("#X restore 310 136 pd routing \\\\$0;"), "subpatch restore dollar argument did not round-trip in a mixed patch");
assert(mixedOut.includes("#X obj 30 128 s \\\\$0-routed;"), "inner subpatch $0 sender did not round-trip in a mixed patch");
assert(mixedOut.includes("#X obj 520 302 array get \\\\$0-wave;"), "array get dollar argument did not round-trip in a mixed patch");
assert(mixedOut.includes("#X obj 520 348 text get \\\\$0-seq;"), "text get dollar argument did not round-trip in a mixed patch");
assert(mixedOut.indexOf("#X declare") < mixedOut.indexOf("#X connect"), "declare line should stay before connections");
assert(objects.some(o => isTextStore(o) && String(o.text || "").includes("$0-seq")), "text define store was not recognized inside the mixed patch");
assert(objects.some(o => isArrayLike(o) && o.arrayName === "$0-wave"), "array store was not recognized inside the mixed patch");
assert(objects.some(o => o.sourceKind === "subpatch" && o.subpatchGraphOnParent), "GOP subpatch was not recognized inside the mixed patch");
const mixedTextStore = objects.find(o => isTextStore(o));
assert(openSourceObject(objects.indexOf(mixedTextStore)), "text define stores should open in the source editor");
sourceText.value = "#X obj 46 98 text define -k \\\\$0-seq, f 31;\\n#A set edited 1 \\\\; edited 2;\\n";
toggleSource(document.getElementById("source"));
assert(serialize().includes("#X obj 46 98 text define -k \\\\$0-seq, f 31;"), "edited text define header coordinates/width hint did not serialize after source editing");
assert(serialize().includes("#A set edited 1 \\\\; edited 2;"), "edited text define data did not serialize after source editing");
parsePatch(mixedFixture,false);
selectedSet = new Set([objects.findIndex(o => canonicalName(o) === "clone"), objects.findIndex(o => o.sourceKind === "subpatch"), objects.findIndex(o => canonicalName(o) === "array get")]);
selected = selectedIndices()[0];
assert(copySelection(), "mixed connected Pd objects should copy");
assert(clipboard.patchText && clipboard.patchText.includes("#X obj 36 284 clone voice 4 \\\\$0-voice;"), "copied Pd selection should expose Pd patch text");
assert(clipboard.patchText.includes("#X restore 310 136 pd routing \\\\$0;"), "copied Pd selection text should preserve selected subpatch source");
assert(clipboard.patchText.includes("#X obj 520 302 array get \\\\$0-wave;"), "copied Pd selection text should preserve selected object arguments");
assert(clipboard.patchText.includes("#X connect 0 0 1 0;"), "copied Pd selection text should remap internal selected connections");
assert(!clipboard.patchText.includes("#X connect 5 0 6 0;"), "copied Pd selection text should not leak original document connection indices");
assert(pasteSelection(), "mixed connected Pd objects should paste");
const pastedOut = serialize();
assert(pastedOut.includes("#X obj 66 314 clone voice 4 \\\\$0-voice;"), "pasted clone did not preserve dollar arguments and offset: " + pastedOut);
assert(pastedOut.includes("#X restore 340 166 pd routing \\\\$0;"), "pasted subpatch did not preserve restore dollar arguments and offset: " + pastedOut);
assert(pastedOut.includes("#X obj 550 332 array get \\\\$0-wave;"), "pasted array get did not preserve dollar arguments and offset: " + pastedOut);
assert(pastedOut.includes("#X connect 10 0 11 0;"), "internal pasted connection was not remapped to pasted objects");
parsePatch("#N canvas 20 30 420 260 10;\\n"
 + "#X obj 20 20 loadbang;\\n"
 + "#X msg 20 70 bang;\\n"
 + "#X obj 20 120 print copied;\\n"
 + "#X connect 0 0 1 0;\\n"
 + "#X connect 1 0 2 0;\\n",false);
selectedSet = new Set([2, 0, 1]);
assert(selectedIndices().join(",") === "0,1,2", "selected indices should follow Pd document order, not click order");
lastCanvasPoint = null;
assert(copySelection() && pasteSelection(), "reversed selected Pd objects should copy/paste");
assert(clipboard.patchText.includes("#X connect 0 0 1 0;") && clipboard.patchText.includes("#X connect 1 0 2 0;"), "copied Pd patch text should remap connections in selected document order");
assert(clipboard.patchText.indexOf("#X obj 20 20 loadbang;") < clipboard.patchText.indexOf("#X msg 20 70 bang;"), "copied Pd patch text should keep objects in document order");
const orderedPasteOut = serialize();
assert(orderedPasteOut.includes("#X connect 3 0 4 0;") && orderedPasteOut.includes("#X connect 4 0 5 0;"), "copy/paste should remap internal connections using document order");
assert(orderedPasteOut.includes("#X obj 50 50 loadbang;") && orderedPasteOut.includes("#X msg 50 100 bang;"), "internal paste should keep the legacy offset when there is no active canvas point");
parsePatch("#N canvas 20 30 420 260 10;\\n"
 + "#X obj 20 20 loadbang;\\n"
 + "#X msg 20 70 bang;\\n"
 + "#X obj 20 120 print copied;\\n"
 + "#X connect 0 0 1 0;\\n"
 + "#X connect 1 0 2 0;\\n",false);
selectedSet = new Set([0, 1, 2]);
selected = 0;
lastCanvasPoint = { x: 220, y: 160 };
assert(copySelection() && pasteSelection(), "internal Pd selection should paste near the active canvas point");
const pointedInternalPasteOut = serialize();
assert(pointedInternalPasteOut.includes("#X obj 220 160 loadbang;") && pointedInternalPasteOut.includes("#X msg 220 210 bang;"), "internal paste should align selected objects to the active canvas point");
assert(pointedInternalPasteOut.includes("#X connect 3 0 4 0;") && pointedInternalPasteOut.includes("#X connect 4 0 5 0;"), "pointed internal paste should still remap internal connections");
function objectMouseTarget(index){
 return { closest(selector){ return selector === ".box" ? { getAttribute: key => key === "data-index" ? String(index) : "" } : null; } };
}
function canvasMouse(type, x, y, extra = {}){
 return Object.assign({ type, button:0, clientX:x, clientY:y, altKey:false, metaKey:false, ctrlKey:false, shiftKey:false, target:null, preventDefault(){} }, extra);
}
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n",false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{altKey:true,target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mouseup",45,45,{altKey:true,target:objectMouseTarget(0)}));
assert(objects.length === 1, "Alt-click without a drag should not duplicate a Pd object");
assert(!backendEvents.some(event => event.name === "patchChanged"), "Alt-click without a drag should not emit a changed Pd patch");
commitInlineEdit(false);
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n",false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{altKey:true,target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mousemove",70,65,{altKey:true,target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mouseup",70,65,{altKey:true,target:objectMouseTarget(1)}));
assert(objects.length === 2, "Alt-drag should duplicate the selected Pd object once");
assert(objects[0].x === 40 && objects[0].y === 40, "Alt-drag should leave the original Pd object in place");
assert(objects[1].x === 65 && objects[1].y === 60, "Alt-drag should move the duplicated Pd object by the drag distance");
assert(backendEvents.some(event => event.name === "patchChanged"), "Alt-drag duplication should emit a changed Pd patch");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n",false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mousemove",-10,-5,{target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mouseup",-10,-5,{target:objectMouseTarget(0)}));
assert(objects[0].x === -15 && objects[0].y === -10, "Pd object dragging should allow negative canvas coordinates instead of clamping at zero");
assert(backendEvents.some(event => event.name === "patchChanged"), "dragging a Pd object into negative canvas space should emit a changed patch");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n",false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mousemove",80,75,{target:objectMouseTarget(0)}));
svg.dispatchEvent({ type: "mouseleave" });
assert(dragging !== null && objects[0].x === 75 && objects[0].y === 70, "leaving the Pd canvas should preserve a live object drag until release");
document.dispatchEvent(canvasMouse("mouseup",80,75));
assert(dragging === null && backendEvents.some(event => event.name === "patchChanged"), "releasing outside the Pd canvas should finalize the moved object drag");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n",false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{target:objectMouseTarget(0)}));
document.dispatchEvent(canvasMouse("mousemove",140,115));
document.dispatchEvent(canvasMouse("mouseup",140,115));
assert(objects[0].x === 135 && objects[0].y === 110, "Pd object drags should continue when the pointer leaves the canvas");
assert(backendEvents.some(event => event.name === "patchChanged"), "cross-boundary Pd object drags should commit their patch change");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n",false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mousemove",80,75,{target:objectMouseTarget(0)}));
cancelCanvasGesture();
assert(dragging === null && objects[0].x === 75 && objects[0].y === 70, "losing Pd window focus should keep and finalize a moved object");
assert(backendEvents.some(event => event.name === "patchChanged"), "focus-loss gesture cleanup should commit a moved Pd object");
parsePatch("#N canvas 20 30 360 220 10;\\n#X floatatom 40 40 5 0 0 0 - - - 2;\\n",false);
setEditMode(false);
backendEvents.length = 0;
svg.dispatchEvent(canvasMouse("mousedown",45,45,{target:objectMouseTarget(0)}));
svg.dispatchEvent(canvasMouse("mousemove",45,10,{target:objectMouseTarget(0)}));
svg.dispatchEvent({ type: "mouseleave" });
assert(guiDrag !== null && objects[0].value > 2, "leaving the Pd canvas should preserve a live GUI value drag until release");
document.dispatchEvent(canvasMouse("mouseup",45,10));
assert(guiDrag === null && backendEvents.some(event => event.name === "patchChanged"), "releasing outside the Pd canvas should finalize the GUI value drag");
setEditMode(true);
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 osc~ 220;\\n#X obj 180 40 dac~;\\n",false);
selected = 0;
selectedSet = new Set([0]);
svg.dispatchEvent(canvasMouse("mousedown",160,25,{shiftKey:true}));
svg.dispatchEvent(canvasMouse("mousemove",280,110,{shiftKey:true}));
svg.dispatchEvent(canvasMouse("mouseup",280,110,{shiftKey:true}));
assert(selectedIndices().join(",") === "0,1", "Shift-marquee should add touched Pd objects to the existing selection");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 loadbang;\\n",false);
lastCanvasPoint = null;
assert(pastePatchText("#N canvas 0 0 240 180 10;\\n#X msg 10 10 bang;\\n#X obj 10 60 print from-pd;\\n#X connect 0 0 1 0;\\n"), "raw Pd patch text should paste into the current canvas");
const rawPasteOut = serialize();
assert(rawPasteOut.includes("#X obj 20 20 loadbang;"), "raw Pd text paste should preserve existing canvas objects");
assert(rawPasteOut.includes("#X msg 40 40 bang;") && rawPasteOut.includes("#X obj 40 90 print from-pd;"), "raw Pd text paste should offset imported objects");
assert(rawPasteOut.includes("#X connect 1 0 2 0;"), "raw Pd text paste should remap imported internal connections");
assert(!rawPasteOut.includes("#X connect 0 0 1 0;"), "raw Pd text paste should not leak snippet-local connection indices into the destination");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 loadbang;\\n",false);
lastCanvasPoint = { x: 180, y: 140 };
assert(pastePatchText("#N canvas 0 0 240 180 10;\\n#X msg 10 10 bang;\\n#X obj 10 60 print from-pd;\\n#X connect 0 0 1 0;\\n"), "raw Pd patch text should paste near the active canvas point");
const pointedPasteOut = serialize();
assert(pointedPasteOut.includes("#X msg 180 140 bang;") && pointedPasteOut.includes("#X obj 180 190 print from-pd;"), "raw Pd text paste should align the snippet top-left to the active canvas point");
assert(pointedPasteOut.includes("#X connect 1 0 2 0;"), "pointed raw Pd text paste should still remap imported internal connections");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 loadbang;\\n",false);
lastCanvasPoint = null;
assert(pastePatchText("#N canvas 0 0 320 240 10;\\n#X array pasted_graph 8 float 0;\\n#A 0 0 1 0 -1 0 1 0 -1;\\n#X coords 0 1 7 -1 200 140 1 0 0;\\n#X restore 120 90 graph;\\n"), "raw Pd graph-array text should paste into the current canvas");
const graphTextPasteOut = serialize();
assert(graphTextPasteOut.includes("#X array pasted_graph 8 float 0;"), "raw Pd graph-array paste should preserve array source");
assert(graphTextPasteOut.includes("#X coords 0 1 7 -1 200 140 1 0 0;"), "raw Pd graph-array paste should preserve visual coords");
assert(graphTextPasteOut.includes("#X restore 150 120 graph;"), "raw Pd graph-array paste should offset the graph restore wrapper");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
lastCanvasPoint = { x: 144, y: 188 };
assert(pastePatchText("custom_external~ 440"), "plain clipboard object text should paste as a Pd object box");
assert(objects.length === 1 && objects[0].text === "custom_external~ 440" && objects[0].x === 144 && objects[0].y === 188, "plain clipboard object text did not paste at the preferred canvas point");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
lastCanvasPoint = { x: 166, y: 202 };
assert(pastePatchText("\\n  expr $f1 + $f2\\nsecond line ignored"), "multiline plain clipboard text should paste its first non-empty object line");
assert(objects.length === 1 && objects[0].text === "expr $f1 + $f2" && objects[0].x === 166 && objects[0].y === 202, "multiline plain clipboard paste should use the first non-empty object line");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
assert(!pastePatchText("#N canvas 0 0 100 100 10;"), "canvas-only Pd text should not paste as a bogus object box");
const guiPropertyFixture = "#N canvas 20 30 620 360 10;\\n"
 + "#X obj 20 20 cnv 15 100 60 oldRecv oldSend oldLabel 20 12 0 14 #e0e0e0 #404040 #000000;\\n"
 + "#X obj 20 120 hsl 128 20 0 127 0 0 oldSliderRecv oldSliderSend oldSlider -2 -10 0 12 #fcfcfc #000000 #000000 12 1;\\n";
parsePatch(guiPropertyFixture,false);
const canvasGui = objects.find(o => canonicalName(o) === "cnv");
const sliderGui = objects.find(o => canonicalName(o) === "hsl");
assert(canvasGui && guiReceiveName(canvasGui) === "oldRecv" && guiSendName(canvasGui) === "oldSend", "canvas GUI receive/send names were not parsed");
assert(sliderGui && guiReceiveName(sliderGui) === "oldSliderRecv" && guiSendName(sliderGui) === "oldSliderSend", "slider receive/send names were not parsed");
assert(boxSize(canvasGui).w === 100 && boxSize(canvasGui).h === 60, "imported canvas GUI did not use its Pd width and height");
assert(boxSize(sliderGui).w === 128 && boxSize(sliderGui).h === 20, "imported slider GUI did not use its Pd width and height");
assert(guiLabelOffsetValue(canvasGui,"x") === "20" && guiLabelOffsetValue(canvasGui,"y") === "12" && guiFontFaceValue(canvasGui) === "0" && guiFontSizeValue(canvasGui) === "14", "canvas label offset/font metadata was not readable");
assert(guiLabelOffsetValue(sliderGui,"x") === "-2" && guiLabelOffsetValue(sliderGui,"y") === "-10" && guiFontFaceValue(sliderGui) === "0" && guiFontSizeValue(sliderGui) === "12", "slider label offset/font metadata was not readable");
assert(openPropertiesForObject(objects.indexOf(canvasGui)), "GUI properties should open for editable Pd GUI objects");
document.getElementById("propLabel").value = "temporaryLabel";
document.getElementById("props").dispatchEvent({ type:"keydown", key:"Escape", preventDefault() { this.defaultPrevented = true; } });
assert(!document.getElementById("props").classList.contains("open"), "Escape should close the GUI properties panel");
assert(guiLabelText(canvasGui) === "oldLabel", "closing GUI properties with Escape should not apply typed-but-unapplied changes");
assert(openPropertiesForObject(objects.indexOf(canvasGui)), "GUI properties should reopen after Escape");
objectMenu.classList.add("open");
menuPoint = { x: 10, y: 20 };
menuTarget = 0;
menuCordTarget = 0;
patchCord = { from: 0, out: 0, to: { x: 100, y: 100 }, start: { x: 20, y: 20 }, dirty: false, moved: true, mode: "fromOutlet" };
marquee = { start: { x: 0, y: 0 }, current: { x: 20, y: 20 } };
dragging = { start: { x: 0, y: 0 }, items: [], moved: true };
guiDrag = { index: 0 };
panning = { x: 0, y: 0, vx: 0, vy: 0 };
clickEditCandidate = { index: 0 };
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 30 40 osc~ 220;\\n", false);
assert(!document.getElementById("props").classList.contains("open") && document.getElementById("props").dataset.index === "-1", "loading a new Pd patch should close stale GUI properties panels");
assert(!objectMenu.classList.contains("open") && menuPoint === null && menuTarget === -1 && menuCordTarget === -1, "loading a new Pd patch should close stale context menus");
assert(patchCord === null && marquee === null && dragging === null && guiDrag === null && panning === null && clickEditCandidate === null, "loading a new Pd patch should clear stale drag/cable/marquee state");
parsePatch(guiPropertyFixture,false);
assert(openPropertiesForObject(0), "GUI properties should open before clear stale-state regression");
objectMenu.classList.add("open");
menuPoint = { x: 11, y: 22 };
menuTarget = 0;
patchCord = { from: 0, out: 0, to: { x: 100, y: 100 }, start: { x: 20, y: 20 }, dirty: false, moved: true, mode: "fromOutlet" };
marquee = { start: { x: 0, y: 0 }, current: { x: 20, y: 20 } };
dragging = { start: { x: 0, y: 0 }, items: [], moved: true };
guiDrag = { index: 0 };
panning = { x: 0, y: 0, vx: 0, vy: 0 };
clickEditCandidate = { index: 0 };
document.getElementById("clear").onclick();
assert(objects.length === 0 && connections.length === 0, "Pd clear should empty the current canvas");
assert(!document.getElementById("props").classList.contains("open") && !objectMenu.classList.contains("open"), "Pd clear should close stale panels and menus");
assert(patchCord === null && marquee === null && dragging === null && guiDrag === null && panning === null && clickEditCandidate === null, "Pd clear should clear stale drag/cable/marquee state");
parsePatch(guiPropertyFixture,false);
const canvasGuiReloaded = objects.find(o => canonicalName(o) === "cnv");
const sliderGuiReloaded = objects.find(o => canonicalName(o) === "hsl");
assert(canvasGuiReloaded && sliderGuiReloaded, "GUI property fixture did not reload after stale-properties close test");
const canvasGuiIndex = objects.indexOf(canvasGuiReloaded);
const sliderGuiIndex = objects.indexOf(sliderGuiReloaded);
applyGuiProperties(canvasGuiIndex, { label:"newLabel", receive:"newRecv", send:"newSend", width:"180", height:"75", labelX:"24", labelY:"16", fontFace:"1", fontSize:"18", bg:"#123456", fg:"#abcdef", labelColor:"#fedcba" });
applyGuiProperties(sliderGuiIndex, { label:"newSlider", receive:"newSliderRecv", send:"newSliderSend", width:"160", height:"24", min:"-12", max:"12", labelX:"5", labelY:"-14", fontFace:"2", fontSize:"16", init:"1", steady:"0", bg:"#202020", fg:"#ff8844", labelColor:"#88ff44" });
assert(boxSize(canvasGuiReloaded).w === 180 && boxSize(canvasGuiReloaded).h === 75, "edited canvas GUI did not use its Pd width and height");
assert(boxSize(sliderGuiReloaded).w === 160 && boxSize(sliderGuiReloaded).h === 24, "edited slider GUI did not use its Pd width and height");
const guiOut = serialize();
assert(guiOut.includes("#X obj 20 20 cnv 15 180 75 newRecv newSend newLabel 24 16 1 18 #123456 #abcdef #fedcba;"), "canvas GUI properties did not export with edited receive/send/label placement/font/size/colors: " + guiOut);
assert(guiOut.includes("#X obj 20 120 hsl 160 24 -12 12 0 1 newSliderRecv newSliderSend newSlider 5 -14 2 16 #202020 #ff8844 #88ff44"), "slider GUI properties did not export with edited receive/send/range/label placement/font/size/colors: " + guiOut);
parsePatch("#N canvas 20 30 620 260 10;\\n"
 + "#X obj 20 20 bng 20 250 50 0 empty empty label\\\\ with\\\\ spaces 0 -10 0 12 #fcfcfc #000000 #000000;\\n",false);
assert(guiLabelText(objects[0]) === "label with spaces", "escaped-space GUI label was not readable");
assert(serialize().includes("label\\\\ with\\\\ spaces"), "escaped-space GUI label did not serialize as one Pd atom");
const guiAliasFixture = "#N canvas 20 30 620 360 10;\\n"
 + "#X obj 20 20 my_canvas 15 90 45 aliasRecv aliasSend aliasLabel 10 8 0 12 #eeeeee #111111 #222222;\\n"
 + "#X obj 20 100 my_numbox 5 20 -10 10 0 0 nbRecv nbSend nbLabel 0 -8 0 12 #ffffff #000000 #444444 3 256;\\n"
 + "#X obj 20 160 rdb 20 1 0 4 radioRecv radioSend radioLabel 0 -8 0 12 #ffffff #000000 #444444 2;\\n"
 + "#X obj 160 160 vdl 20 1 0 4 dialRecv dialSend dialLabel 0 -8 0 12 #ffffff #000000 #444444 1;\\n";
parsePatch(guiAliasFixture,false);
const aliasCanvas = objects.find(o => canonicalName(o) === "my_canvas");
const aliasNumbox = objects.find(o => canonicalName(o) === "my_numbox");
const aliasRadio = objects.find(o => canonicalName(o) === "rdb");
const aliasDial = objects.find(o => canonicalName(o) === "vdl");
assert([aliasCanvas, aliasNumbox, aliasRadio, aliasDial].every(Boolean), "Pd GUI aliases were not imported");
assert([aliasCanvas, aliasNumbox, aliasRadio, aliasDial].every(o => isKnownObject(o) && isGuiName(canonicalName(o))), "Pd GUI aliases should be known GUI objects");
assert(guiBaseName(canonicalName(aliasCanvas)) === "cnv" && guiReceiveName(aliasCanvas) === "aliasRecv" && guiSendName(aliasCanvas) === "aliasSend", "my_canvas alias did not behave as cnv");
assert(guiBaseName(canonicalName(aliasNumbox)) === "nbx" && guiReceiveName(aliasNumbox) === "nbRecv" && guiSendName(aliasNumbox) === "nbSend", "my_numbox alias did not behave as nbx");
assert(guiBaseName(canonicalName(aliasRadio)) === "hradio" && guiBaseName(canonicalName(aliasDial)) === "vradio", "radio aliases did not normalize internally");
assert(boxSize(aliasRadio).w === 80 && boxSize(aliasRadio).h === 20, "imported horizontal radio alias did not use Pd cell/count dimensions");
assert(boxSize(aliasDial).w === 20 && boxSize(aliasDial).h === 80, "imported vertical radio alias did not use Pd cell/count dimensions");
const aliasOut = serialize();
assert(aliasOut.includes("#X obj 20 20 my_canvas 15 90 45 aliasRecv aliasSend aliasLabel 10 8 0 12 #eeeeee #111111 #222222;"), "my_canvas alias did not preserve its original object name on export");
assert(aliasOut.includes("#X obj 20 100 my_numbox 5 20 -10 10 0 0 nbRecv nbSend nbLabel 0 -8 0 12 #ffffff #000000 #444444 3 256;"), "my_numbox alias did not preserve its original object name on export");
const extendedGuiAliasFixture = "#N canvas 20 30 760 360 10;\\n"
 + "#X obj 20 20 toggle 24 1 tglRecv tglSend tglLabel 0 -8 0 12 #fefefe #111111 #222222 1 1;\\n"
 + "#X obj 20 80 hslider 140 18 0 1 0 0 hsRecv hsSend hsLabel -2 -8 0 12 #eeeeee #333333 #444444 0 1;\\n"
 + "#X obj 20 140 vslider 18 140 -1 1 0 0 vsRecv vsSend vsLabel 0 -8 0 12 #eeeeee #333333 #444444 0 1;\\n"
 + "#X obj 240 20 hdl 18 1 0 6 hdlRecv hdlSend hdlLabel 0 -8 0 12 #ffffff #000000 #444444 3;\\n"
 + "#X obj 240 80 radiobutton 18 1 0 5 rbRecv rbSend rbLabel 0 -8 0 12 #ffffff #000000 #444444 2;\\n"
 + "#X obj 240 140 radiobut 18 1 0 4 rbutRecv rbutSend rbutLabel 0 -8 0 12 #ffffff #000000 #444444 1;\\n";
parsePatch(extendedGuiAliasFixture,false);
const aliasToggle = objects.find(o => canonicalName(o) === "toggle");
const aliasHSlider = objects.find(o => canonicalName(o) === "hslider");
const aliasVSlider = objects.find(o => canonicalName(o) === "vslider");
const aliasHdl = objects.find(o => canonicalName(o) === "hdl");
const aliasRadioButton = objects.find(o => canonicalName(o) === "radiobutton");
const aliasRadioBut = objects.find(o => canonicalName(o) === "radiobut");
assert([aliasToggle, aliasHSlider, aliasVSlider, aliasHdl, aliasRadioButton, aliasRadioBut].every(Boolean), "extended Pd GUI aliases were not imported");
assert(guiBaseName(canonicalName(aliasToggle)) === "tgl" && guiBaseName(canonicalName(aliasHSlider)) === "hsl" && guiBaseName(canonicalName(aliasVSlider)) === "vsl", "toggle/slider aliases did not normalize internally");
assert([aliasHdl, aliasRadioButton, aliasRadioBut].every(o => guiBaseName(canonicalName(o)) === "hradio"), "radio button aliases did not normalize internally");
const extendedAliasOut = serialize();
assert(extendedAliasOut.includes("#X obj 20 20 toggle 24 1 tglRecv tglSend tglLabel 0 -8 0 12 #fefefe #111111 #222222 1 1;"), "toggle alias did not preserve its original object name on export");
assert(extendedAliasOut.includes("#X obj 20 80 hslider 140 18 0 1 0 0 hsRecv hsSend hsLabel -2 -8 0 12 #eeeeee #333333 #444444 0 1;"), "hslider alias did not preserve its original object name on export");
assert(extendedAliasOut.includes("#X obj 20 140 vslider 18 140 -1 1 0 0 vsRecv vsSend vsLabel 0 -8 0 12 #eeeeee #333333 #444444 0 1;"), "vslider alias did not preserve its original object name on export");
assert(extendedAliasOut.includes("#X obj 240 20 hdl 18 1 0 6 hdlRecv hdlSend hdlLabel 0 -8 0 12 #ffffff #000000 #444444 3;"), "hdl alias did not preserve its original object name on export");
assert(extendedAliasOut.includes("#X obj 240 80 radiobutton 18 1 0 5 rbRecv rbSend rbLabel 0 -8 0 12 #ffffff #000000 #444444 2;"), "radiobutton alias did not preserve its original object name on export");
assert(extendedAliasOut.includes("#X obj 240 140 radiobut 18 1 0 4 rbutRecv rbutSend rbutLabel 0 -8 0 12 #ffffff #000000 #444444 1;"), "radiobut alias did not preserve its original object name on export");
interactGui(objects.indexOf(aliasHSlider), { x: aliasHSlider.x + 140, y: aliasHSlider.y + 9 });
interactGui(objects.indexOf(aliasRadioButton), { x: aliasRadioButton.x + 72, y: aliasRadioButton.y + 9 });
const interactedAliasOut = serialize();
assert(interactedAliasOut.includes("#X obj 20 80 hslider 140 18 0 1 0 0 hsRecv hsSend hsLabel -2 -8 0 12 #eeeeee #333333 #444444 1 1;"), "interacted hslider alias did not preserve alias name and update value");
assert(interactedAliasOut.includes("#X obj 240 80 radiobutton 18 1 0 5 rbRecv rbSend rbLabel 0 -8 0 12 #ffffff #000000 #444444 4;"), "interacted radiobutton alias did not preserve alias name and update value");
const atomFixture = "#N canvas 20 30 420 240 10;\\n"
 + "#X floatatom 20 20 7 -24 24 2 pitch pitchIn pitchOut 13;\\n"
 + "#X symbolatom 20 80 12 0 0 1 name nameIn nameOut 11;\\n"
 + "#X listbox 20 140 20 0 0 0 steps stepsIn stepsOut 10;\\n";
parsePatch(atomFixture,false);
const floatAtom = objects.find(o => o.sourceKind === "floatatom");
const symbolAtom = objects.find(o => o.sourceKind === "symbolatom");
const listBox = objects.find(o => o.sourceKind === "listbox");
assert(floatAtom && floatAtom.value === 0, "floatatom saved font-size metadata should not be treated as its current value");
assert(symbolAtom && symbolAtom.symbolValue === "", "symbolatom saved metadata should not be treated as its current symbol");
assert(listBox && listBox.symbolValue === "" && isKnownObject(listBox), "listbox should import as a known atom box without treating metadata as current value");
assert(boxSize(listBox).w > boxSize(floatAtom).w && boxSize(symbolAtom).w > boxSize(floatAtom).w, "Pd atom boxes should size from their saved width metadata");
floatAtom.value = 19;
symbolAtom.symbolValue = "played";
listBox.symbolValue = "one two three";
const atomOut = serialize();
assert(atomOut.includes("#X floatatom 20 20 7 -24 24 2 pitch pitchIn pitchOut 13;"), "floatatom value edit should not overwrite saved atom metadata: " + atomOut);
assert(atomOut.includes("#X symbolatom 20 80 12 0 0 1 name nameIn nameOut 11;"), "symbolatom value edit should not overwrite saved atom metadata: " + atomOut);
assert(atomOut.includes("#X listbox 20 140 20 0 0 0 steps stepsIn stepsOut 10;"), "listbox value edit should not overwrite saved atom metadata: " + atomOut);
assert(canInlineEditObject(objects.indexOf(floatAtom)) && canInlineEditObject(objects.indexOf(symbolAtom)) && canInlineEditObject(objects.indexOf(listBox)), "Pd atom boxes should be inline editable");
beginInlineEdit(objects.indexOf(listBox), svgToScreen(listBox.x, listBox.y).x + 8, false);
assert(editing === objects.indexOf(listBox) && inlineEdit.style.display === "block", "listbox did not enter inline edit");
inlineEdit.value = "alpha beta gamma";
commitInlineEdit(true);
assert(listBox.symbolValue === "alpha beta gamma", "listbox inline edit did not update its live text");
assert(serialize().includes("#X listbox 20 140 20 0 0 0 steps stepsIn stepsOut 10;"), "listbox inline edit should not overwrite saved metadata");
beginInlineEdit(objects.indexOf(floatAtom));
inlineEdit.value = "symbolatom";
commitInlineEdit(true);
assert(floatAtom.sourceKind === "symbolatom" && floatAtom.text === "symbolatom" && canInlineEditObject(objects.indexOf(floatAtom)), "float atom did not convert to symbolatom when typed");
assert(serialize().includes("#X symbolatom 20 20 7 -24 24 2 pitch pitchIn pitchOut 13;"), "converted symbolatom did not preserve atom metadata");
assert(boxSize(floatAtom).w === boxSize({ ...floatAtom, sourceKind: "symbolatom" }).w, "converted symbolatom should keep the atom metadata width");
beginInlineEdit(objects.indexOf(floatAtom));
inlineEdit.value = "listbox";
commitInlineEdit(true);
assert(floatAtom.sourceKind === "listbox" && floatAtom.text === "listbox", "symbolatom did not convert to listbox when typed");
assert(serialize().includes("#X listbox 20 20 7 -24 24 2 pitch pitchIn pitchOut 13;"), "converted listbox did not serialize as listbox");
beginInlineEdit(objects.indexOf(floatAtom));
inlineEdit.value = "floatatom";
commitInlineEdit(true);
assert(floatAtom.sourceKind === "floatatom" && floatAtom.text === "floatatom" && Number.isFinite(floatAtom.value), "listbox did not convert back to floatatom when typed");
assert(serialize().includes("#X floatatom 20 20 7 -24 24 2 pitch pitchIn pitchOut 13;"), "converted floatatom did not serialize as floatatom");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
menuPoint = { x: 44, y: 55 };
menuTarget = -1;
menuCordTarget = -1;
menuSearch.value = "";
menuActive = 0;
renderObjectMenu();
assert(menuActions.length === menuList.querySelectorAll(".menuItem").length, "each visible Pd context menu item should map to exactly one keyboard action");
chooseActiveMenuItem();
assert(objects.length === 1 && objects[0].kind === "obj", "keyboard object menu did not activate the basic Object put item");
assert(canInlineEditObject(0), "new Pd object should be inline editable");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
showObjectMenu({
 type: "contextmenu",
 button: 2,
 clientX: 72,
 clientY: 96,
 target: { closest() { return null; } },
 preventDefault() {}
});
assert(objectMenu.classList.contains("open") && menuPoint.x === 72 && menuPoint.y === 96, "right-click should open the Pd object menu at the clicked canvas point");
menuSearch.value = "osc~";
renderObjectMenu();
chooseActiveMenuItem();
assert(objects.length === 1 && canonicalName(objects[0]) === "osc~" && objects[0].x === 72 && objects[0].y === 96, "right-click object menu should place the chosen Pd object at the clicked canvas point");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addPickedObjectAt("message", { x: 64, y: 88 });
assert(objects.length === 1 && objects[0].kind === "msg" && objects[0].text === "", "picked message should create an empty Pd message box, not an ordinary [message] object");
assert(serialize().includes("#X msg 64 88 ;"), "picked message box did not serialize as #X msg");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addPickedObjectAt("pd", { x: 88, y: 104 });
assert(objects.length === 1 && objects[0].sourceKind === "subpatch" && objects[0].text === "pd subpatch", "picked pd should create a subpatch, not an ordinary [pd] object");
const pickedPdOut = serialize();
assert(pickedPdOut.includes("#N canvas 120 120 520 360 subpatch 0;") && pickedPdOut.includes("#X restore 88 104 pd subpatch;"), "picked pd did not serialize as a nested subpatch");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addPickedObjectAt("page", { x: 90, y: 106 });
assert(objects.length === 1 && objects[0].sourceKind === "subpatch" && objects[0].text === "page subpatch", "picked page should create a Pd page subcanvas");
assert(serialize().includes("#X restore 90 106 page subpatch;"), "picked page did not serialize with a page restore line");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addPickedObjectAt("array", { x: 92, y: 116 });
assert(objects.length === 1 && objects[0].sourceKind === "array" && objects[0].arrayName === "array1" && openSourceObject(0), "picked array should create editable visual array storage");
toggleSource(document.getElementById("source"));
assert(serialize().includes("#X array array1 100 float 3;"), "picked array did not serialize as Pd array storage");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addPickedObjectAt("table", { x: 96, y: 120 });
assert(objects.length === 1 && objects[0].sourceKind === "table" && objects[0].arrayName === "table1" && openSourceObject(0), "picked table should create editable table storage");
toggleSource(document.getElementById("source"));
assert(serialize().includes("#X obj 96 120 table table1 100;"), "picked table did not serialize as a Pd table object");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
objectSearch.value = "osc";
populateObjectPicker(objectSearch.value);
document.getElementById("addPicked").onclick();
assert(objects.length === 1 && canonicalName(objects[0]) === "osc~", "toolbar partial object search should still place the selected catalogue match");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
objectSearch.value = "custom_external~ 440";
populateObjectPicker(objectSearch.value);
document.getElementById("addPicked").onclick();
assert(objects.length === 1 && objects[0].text === "custom_external~ 440", "toolbar object add should preserve typed custom Pd object text when no catalogue match exists");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
objectSearch.value = "TrIgGeR";
populateObjectPicker(objectSearch.value);
document.getElementById("addPicked").onclick();
assert(objects.length === 1 && canonicalName(objects[0]) === "trigger", "toolbar exact object search should normalize to the catalogue object's spelling");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addPickedObjectAt("gatom", { x: 76, y: 96 });
assert(objects.length === 1 && objects[0].sourceKind === "floatatom" && canInlineEditObject(0), "picked gatom should behave as an editable number atom");
assert(serialize().includes("#X floatatom 76 96 5 0 0 0 - - - 0;"), "picked gatom did not serialize as #X floatatom");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 40 40 gatom;\\n",false);
assert(objects[0] && objects[0].sourceKind === "floatatom" && canInlineEditObject(0), "imported gatom alias should normalize to an editable number atom");
assert(serialize().includes("#X floatatom 40 40 5 0 0 0 - - - 0;"), "imported gatom alias did not serialize as a floatatom");
objects.push({ id: 905, kind: "obj", x: 20, y: 20, text: "bng" });
ensureGuiObject(objects[1]);
assert(!canInlineEditObject(1), "GUI widgets should not enter text edit on simple click");
parsePatch("#N canvas 20 30 360 220 10;\\n"
 + "#X obj 20 20 nbx 5 20 0 12 0 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 4 256;\\n"
 + "#X floatatom 20 70 5 0 0 0 - - - 2;\\n"
 + "#X symbolatom 20 120 10 0 0 0 - - - 0;\\n",false);
assert(canDragGui(0) && canDragGui(1), "Pd number widgets should support drag editing");
assert(!interactGui(2, { x: 22, y: 122 }), "symbol atoms should open text editing instead of fake-triggering on click");
interactGui(0, { x: 24, y: 2 }, { start: { x: 24, y: 20 }, startValue: 4, shiftKey: false, altKey: false });
assert(objects[0].value === 7, "dragged nbx did not update by Pd-style vertical motion");
interactGui(1, { x: 24, y: 46 }, { start: { x: 24, y: 70 }, startValue: 2, shiftKey: false, altKey: false });
assert(objects[1].value === 6, "dragged floatatom did not update by Pd-style vertical motion");
setEditMode(false);
backendEvents.length = 0;
assert(beginRuntimeValueEdit(0), "run-mode number box should accept typed value entry");
inlineEdit.value = "11";
commitInlineEdit(true);
assert(objects[0].value === 11 && !runtimeValueEditing && editing === -1, "typed run-mode number value was not committed");
assert(backendEvents.some(event => event.name === "guiTriggered" && event.payload.selector === "float" && event.payload.atoms[0] === "11"), "typed run-mode number should fire its Pd float value");
backendEvents.length = 0;
assert(beginRuntimeValueEdit(2), "run-mode symbol atom should accept typed value entry");
inlineEdit.value = "typed symbol";
commitInlineEdit(true);
assert(objects[2].symbolValue === "typed symbol", "typed run-mode symbol value was not committed");
assert(backendEvents.some(event => event.name === "guiTriggered" && event.payload.selector === "symbol" && event.payload.atoms[0] === "typed symbol"), "typed run-mode symbol should fire its Pd symbol value");
assert(beginRuntimeValueEdit(1), "run-mode float atom should enter typed value editing");
inlineEdit.value = "99";
commitInlineEdit(false);
assert(objects[1].value === 6 && !runtimeValueEditing, "Escape should cancel run-mode atom value entry");
setEditMode(true);
backendEvents.length = 0;
parsePatch("#N canvas 20 30 360 260 10;\\n"
 + "#X obj 20 20 bng 20 250 50 0 empty empty fire 0 -10 0 12 #fcfcfc #000000 #000000;\\n"
 + "#X obj 20 70 tgl 20 1 empty empty gate 0 -10 0 12 #fcfcfc #000000 #000000 0 1;\\n"
 + "#X symbolatom 20 120 10 0 0 0 - - - 0;\\n"
 + "#X listbox 20 170 20 0 0 0 - - - 0;\\n",false);
objects[2].symbolValue = "alpha";
objects[3].symbolValue = "alpha beta 3";
emitGuiTrigger(0);
emitGuiTrigger(1);
emitGuiTrigger(2);
emitGuiTrigger(3);
const guiEvents = backendEvents.filter(event => event.name === "guiTriggered").map(event => event.payload);
assert(guiEvents[0].selector === "bang" && guiEvents[0].atoms.length === 0, "bang GUI trigger should emit a bang message");
assert(guiEvents[1].selector === "float" && guiEvents[1].atoms[0] === "0", "toggle GUI trigger should emit its numeric float value");
assert(guiEvents[2].selector === "symbol" && guiEvents[2].atoms[0] === "alpha", "symbolatom GUI trigger should emit a symbol message");
assert(guiEvents[3].selector === "list" && guiEvents[3].atoms.join(" ") === "alpha beta 3", "listbox GUI trigger should emit list atoms");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 100 100 osc~ 220;\\n",false);
menuPoint = { x: 100, y: 100 };
menuTarget = 0;
menuCordTarget = -1;
menuSearch.value = "";
menuActive = 0;
renderObjectMenu();
assert(menuActions[menuActive] && menuActions[menuActive].enabled, "context menu should land on an enabled action");
const previousMenuActive = menuActive;
moveMenuActive(1);
assert(menuActions[menuActive] && menuActions[menuActive].enabled && menuActive !== previousMenuActive, "context menu arrows should move between enabled actions");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 100 100 osc~ 220;\\n",false);
const editObject = objects[0];
beginInlineEdit(0, svgToScreen(editObject.x, editObject.y).x + 8 + textWidth("osc"), false);
assert(editing === 0 && inlineEdit.style.display === "block", "existing object did not enter inline edit");
assert(inlineEdit.selectionStart > 0 && inlineEdit.selectionStart < inlineEdit.value.length, "click-to-edit did not place a caret inside the object text");
inlineEdit.value = "phasor~ 110";
commitInlineEdit(true);
assert(objects[0].text === "phasor~ 110", "committed inline edit did not update the object text");
parsePatch("#N canvas 20 30 360 260 10;\\n"
 + "#X obj 100 100 osc~ 220;\\n"
 + "#X obj 100 160 bng 20 250 50 0 empty empty fire 0 -10 0 12 #fcfcfc #000000 #000000;\\n",false);
selectOnly(0);
let enterPrevented = false;
document.dispatchEvent({ type: "keydown", key: "Enter", metaKey: false, ctrlKey: false, target: null, preventDefault(){ enterPrevented = true; } });
assert(enterPrevented && editing === 0 && inlineEdit.style.display === "block", "Return should enter inline editing for a selected Pd text box");
commitInlineEdit(false);
selectOnly(1);
enterPrevented = false;
document.dispatchEvent({ type: "keydown", key: "Enter", metaKey: false, ctrlKey: false, target: null, preventDefault(){ enterPrevented = true; } });
assert(!enterPrevented && editing < 0 && inlineEdit.style.display === "none", "Return should not force inline editing for non-text Pd GUI widgets");
parsePatch("#N canvas 20 30 360 220 10;\\n",false);
addObject("obj",80,80,"");
beginInlineEdit(0);
inlineEdit.value = "";
commitInlineEdit(true);
assert(objects.length === 0 && serialize().indexOf("#X obj 80 80 osc~ 220;") < 0, "empty Pd object edit should disappear instead of becoming a default oscillator");
addObject("msg",80,80,"");
beginInlineEdit(0);
inlineEdit.value = "";
commitInlineEdit(true);
assert(objects.length === 1 && objects[0].kind === "msg" && objects[0].text === "", "empty Pd message box should remain an empty message, not become bang");
assert(serialize().includes("#X msg 80 80 ;"), "empty Pd message box did not serialize as an empty #X msg");
beginInlineEdit(0);
inlineEdit.value = "; target-a $1, target-b $0";
commitInlineEdit(true);
assert(objects[0].text === "; target-a $1, target-b $0", "typed Pd message syntax should remain readable after edit");
assert(serialize().includes("#X msg 80 80 \\\\; target-a \\\\$1\\\\, target-b \\\\$0"), "typed Pd message syntax was not escaped for saved Pd text");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 r a, f 3;\\n#X msg 20 70 tiny, f 80;\\n",false);
beginInlineEdit(0);
inlineEdit.value = "route a-very-long-symbol another-long-symbol";
commitInlineEdit(true);
const widenedOut = serialize();
assert(/#X obj 20 20 route a-very-long-symbol another-long-symbol, f ([1-9][0-9]+);/.test(widenedOut), "edited object did not grow its saved Pd width hint");
beginInlineEdit(1);
inlineEdit.value = "still tiny";
commitInlineEdit(true);
assert(serialize().includes("#X msg 20 70 still tiny, f 80;"), "wide manual Pd width hint should be preserved when edited text still fits");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 100 100 phasor~ 110;\\n",false);
beginInlineEdit(0);
inlineEdit.value = "array typedArray 64 float 3";
commitInlineEdit(true);
assert(objects[0].sourceKind === "array" && objects[0].arrayName === "typedArray" && openSourceObject(0), "typed array object did not become editable Pd array storage");
toggleSource(document.getElementById("source"));
assert(serialize().includes("#X array typedArray 64 float 3;"), "typed array object did not serialize as Pd array storage");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 100 100 phasor~ 110;\\n",false);
beginInlineEdit(0);
inlineEdit.value = "pd typed";
commitInlineEdit(true);
assert(objects[0].sourceKind === "subpatch" && objects[0].text === "pd typed", "typed pd object did not become a subpatch");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 100 100 osc~ 220;\\n",false);
beginInlineEdit(0);
inlineEdit.value = "pd escaped\\\\ name";
commitInlineEdit(true);
assert(objects[0].sourceKind === "subpatch" && objects[0].text === "pd escaped name", "typed escaped-space pd object did not become one named subpatch");
assert(serialize().includes("#X restore 100 100 pd escaped\\\\ name;"), "typed escaped-space pd subpatch did not serialize with the intended name");
objects[0].subpatchText += "\\n#X obj 20 20 print still-inside;";
beginInlineEdit(0);
inlineEdit.value = "pd renamed\\\\ patch";
commitInlineEdit(true);
assert(objects[0].subpatchText.split(/\\r?\\n/)[0].includes("renamed\\\\ patch 0;"), "renaming a Pd subpatch should update the inner canvas title");
assert(objects[0].subpatchText.includes("print still-inside"), "renaming a Pd subpatch should preserve its inner contents");
assert(serialize().includes("#X restore 100 100 pd renamed\\\\ patch;"), "renamed Pd subpatch restore line did not serialize with the new escaped name");
beginInlineEdit(0);
inlineEdit.value = "page typedPage";
commitInlineEdit(true);
assert(objects[0].sourceKind === "subpatch" && objects[0].text === "page typedPage" && serialize().includes("#X restore 100 100 page typedPage;"), "typed page object did not become a page subcanvas");
beginInlineEdit(0);
inlineEdit.value = "osc~ 220";
commitInlineEdit(true);
assert(objects[0].sourceKind !== "subpatch" && objects[0].text === "osc~ 220", "editing a subpatch back into an object left stale subpatch state");
parsePatch(fixture,false);
const gop = objects.find(o => o.sourceKind === "subpatch");
assert(gop && gop.subpatchGraphInfo && gop.subpatchGraphInfo.w === 180 && gop.subpatchGraphInfo.h === 120, "GOP parent dimensions were not parsed from display size");
const items = gopPreviewItems(gop);
assert(items.length >= 2, "GOP preview did not find top-level visible contents");
const interactiveGopFixture = "#N canvas 20 30 640 420 10;\\n"
 + "#N canvas 120 120 360 260 controls 0;\\n"
 + "#X text 4 4 controls;\\n"
 + "#X obj 12 18 tgl 20 0 empty gop-send toggle 0 -10 0 12 #fcfcfc #000000 #000000 0 1;\\n"
 + "#X obj 42 18 hsl 96 20 0 127 0 0 empty empty level -2 -10 0 12 #fcfcfc #000000 #000000 0 1;\\n"
 + "#X msg 42 58 \\$1;\\n"
 + "#X symbolatom 42 78 10 0 0 0 - - - hello;\\n"
 + "#X connect 2 0 3 0;\\n"
 + "#X coords 0 -1 1 1 180 100 1 0 0;\\n"
 + "#X restore 180 120 pd controls;\\n";
parsePatch(interactiveGopFixture,false);
const interactiveGop=objects[0],interactiveItems=gopPreviewItems(interactiveGop);
const toggleItem=interactiveItems.find(item=>guiBaseName(item.name)==="tgl"),sliderItem=interactiveItems.find(item=>guiBaseName(item.name)==="hsl");
assert(toggleItem&&toggleItem.pdIndex===1&&sliderItem&&sliderItem.pdIndex===2, "GOP controls should retain native Pd indices across comments");
const toggleChild=gopItemObject(interactiveGop,toggleItem),togglePoint={x:toggleChild.x+10,y:toggleChild.y+10};
assert(gopGuiAt(0,togglePoint)&&gopGuiAt(0,togglePoint).item.lineIndex===toggleItem.lineIndex, "visible GOP controls should be hit-testable from the parent canvas");
backendEvents.length=0;
assert(interactGopGui({parentIndex:0,lineIndex:toggleItem.lineIndex,start:togglePoint,startValue:0,shiftKey:false,altKey:false},togglePoint), "parent-canvas GOP toggle interaction failed");
assert(objects[0].subpatchText.includes("#X obj 12 18 tgl")&&objects[0].subpatchText.includes(" 1 1;"), "GOP toggle state did not persist in the nested patch source");
const sentGopEvent=backendEvents.find(event=>event.name==="guiTriggered");
assert(sentGopEvent&&sentGopEvent.payload.send.startsWith("otherware_gop_")&&sentGopEvent.payload.patch.includes(" 1 1;")&&sentGopEvent.payload.triggerPatch.includes("#X connect 5 0 1 0;"), "sent GOP controls should enter through the control so Pd drives both its outlet and native send symbol");
const sliderChild=gopItemObject(objects[0],gopPreviewItems(objects[0]).find(item=>item.lineIndex===sliderItem.lineIndex));
const sliderPoint={x:sliderChild.x+boxSize(sliderChild).w*.75,y:sliderChild.y+10};
backendEvents.length=0;
assert(interactGopGui({parentIndex:0,lineIndex:sliderItem.lineIndex,start:sliderPoint,startValue:0,shiftKey:false,altKey:false},sliderPoint), "parent-canvas GOP slider interaction failed");
const unsentGopEvent=backendEvents.find(event=>event.name==="guiTriggered");
assert(unsentGopEvent&&unsentGopEvent.payload.patch.indexOf("otherware_gop_")<0, "temporary GOP receiver leaked into the saved nested patch");
assert(unsentGopEvent.payload.triggerPatch.includes("r otherware_gop_")&&unsentGopEvent.payload.triggerPatch.includes("#X connect 5 0 2 0;"), "unsent GOP control did not receive a playback-only bridge into its native inlet");
const symbolItem=gopPreviewItems(objects[0]).find(item=>item.name==="symbolatom"),symbolChild=gopItemObject(objects[0],symbolItem);
assert(beginGopRuntimeValueEdit({parentIndex:0,item:symbolItem,child:symbolChild}), "visible GOP symbol atoms should open direct run-mode editing");
inlineEdit.value="edited symbol";commitInlineEdit(true);
assert(!objects[0].subpatchText.includes("edited\\ symbol")&&gopPreviewItems(objects[0]).find(item=>item.lineIndex===symbolItem.lineIndex).child.symbolValue==="edited symbol", "typed GOP atom values should remain live without leaking runtime values into Pd patch source");
const symbolGopEvent=backendEvents.filter(event=>event.name==="guiTriggered").pop();
assert(symbolGopEvent&&symbolGopEvent.payload.selector==="symbol"&&symbolGopEvent.payload.atoms[0]==="edited symbol", "typed GOP symbol atoms should trigger their native symbol message");
const liveGuiFixture="#N canvas 20 30 640 420 10;\\n"
 + "#X obj 20 20 tgl 20 0 empty live-toggle toggle 0 -10 0 12 #fcfcfc #000000 #000000 0 1;\\n"
 + "#X obj 70 20 vu 15 120 live-meter meter -1 -10 0 12 #fcfcfc #000000 1 0;\\n"
 + "#X obj 110 20 tgl 20 0 empty \\\\$0-local-state local 0 -10 0 12 #fcfcfc #000000 #000000 0 1;\\n"
 + "#X floatatom 110 70 6 0 0 0 state \\\\$0-local-number empty 0;\\n"
 + "#N canvas 120 120 300 220 live-gop 0;\\n"
 + "#X obj 10 10 hsl 96 20 0 127 0 0 empty live-level level -2 -10 0 12 #fcfcfc #000000 #000000 0 1;\\n"
 + "#X symbolatom 10 40 10 0 0 0 word \\\\$0-gop-symbol empty 0;\\n"
 + "#X coords 0 -1 1 1 150 70 1 0 0;\\n"
 + "#X restore 150 20 pd live-gop;\\n";
parsePatch(liveGuiFixture,false);
assert(monitoredGuiReceivers().join(",")==="$0-gop-symbol,$0-local-number,$0-local-state,live-level,live-meter,live-toggle", "editor should retain static and $0-scoped receive symbols on root and GOP controls and atoms");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"float",atoms:["1"]})&&runtimeDisplayObject(objects[0]).value===1, "incoming Pd floats should update root GUI state");
assert(applyIncomingGuiMessage({receiver:"live-meter",selector:"float",atoms:["-12"]})&&runtimeDisplayObject(objects[1]).value===-12, "incoming Pd meter values should remain live in the editor");
assert(applyIncomingGuiMessage({receiver:"$0-local-state",selector:"float",atoms:["1"]})&&runtimeDisplayObject(objects[2]).value===1, "resolved $0 messages should update their source-spelled GUI receiver");
assert(applyIncomingGuiMessage({receiver:"$0-local-number",selector:"float",atoms:["42.5"]})&&runtimeDisplayObject(objects[3]).value===42.5, "resolved $0 messages should update number atoms");
const liveGop=objects.find(o=>o.subpatchGraphOnParent),liveGopItem=gopPreviewItems(liveGop).find(item=>guiBaseName(item.name)==="hsl");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"float",atoms:["96"]})&&gopPreviewItems(liveGop).find(item=>item.lineIndex===liveGopItem.lineIndex).child.value===96, "incoming Pd floats should update visible GOP controls");
const liveGopSymbol=gopPreviewItems(liveGop).find(item=>item.name==="symbolatom");
assert(applyIncomingGuiMessage({receiver:"$0-gop-symbol",selector:"symbol",atoms:["heard"]})&&gopPreviewItems(liveGop).find(item=>item.lineIndex===liveGopSymbol.lineIndex).child.symbolValue==="heard", "incoming Pd symbols should update visible GOP atoms");
const livePropertySource=serialize();
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"size",atoms:["34"]}), "runtime Pd size messages should reach root IEM controls");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"color",atoms:["#123456","#abcdef","#fedcba"]}), "runtime Pd color messages should reach root IEM controls");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"label",atoms:["runtime label"]}), "runtime Pd label messages should reach root IEM controls");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"label_pos",atoms:["7","-16"]}), "runtime Pd label_pos messages should reach root IEM controls");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"label_font",atoms:["2","18"]}), "runtime Pd label_font messages should reach root IEM controls");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"pos",atoms:["210","140"]}), "runtime Pd pos messages should reach root IEM controls");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"delta",atoms:["5","-3"]}), "runtime Pd delta messages should reach root IEM controls");
let runtimeRoot=runtimeDisplayObject(objects[0]);
assert(runtimeRoot.x===215&&runtimeRoot.y===137&&boxSize(runtimeRoot).w===34&&boxSize(runtimeRoot).h===34, "runtime geometry messages should move and resize root controls coherently");
assert(guiColors(runtimeRoot).bg==="#123456"&&guiColors(runtimeRoot).fg==="#abcdef"&&guiColors(runtimeRoot).label==="#fedcba", "runtime colors should preserve all three native Pd color roles");
assert(guiLabelInfo(runtimeRoot).text==="runtime label"&&guiLabelInfo(runtimeRoot).x===7&&guiLabelInfo(runtimeRoot).y===-16, "runtime label text and position should be reflected by the editor");
assert(runtimeRoot.runtimeGui.fontFace===2&&runtimeRoot.runtimeGui.fontSize===18, "runtime label font messages should retain native face and size settings");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"nonzero",atoms:["5"]})&&applyIncomingGuiMessage({receiver:"live-toggle",selector:"bang",atoms:[]})&&runtimeDisplayObject(objects[0]).value===0, "runtime toggle bangs should follow Pd toggle state");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"bang",atoms:[]})&&runtimeDisplayObject(objects[0]).value===5, "runtime toggle nonzero messages should control the next on value");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"range",atoms:["-1","1"]}), "runtime range messages should reach GOP sliders");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"size",atoms:["120","24"]}), "runtime size messages should reach GOP sliders");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"color",atoms:["#332211","#55aa99","#ffcc44"]}), "runtime color messages should reach GOP controls");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"pos",atoms:["20","25"]}), "runtime pos messages should reach GOP controls in subpatch coordinates");
const runtimeGopItem=gopPreviewItems(liveGop).find(item=>item.lineIndex===liveGopItem.lineIndex),runtimeGopChild=gopItemObject(liveGop,runtimeGopItem);
assert(guiRange(runtimeGopChild).min===-1&&guiRange(runtimeGopChild).max===1, "GOP runtime range should affect its displayed control");
assert(boxSize(runtimeGopChild).w===120&&boxSize(runtimeGopChild).h===24, "GOP runtime size should affect its displayed control");
assert(runtimeGopItem.runtimeState.x===20&&runtimeGopItem.runtimeState.y===25, "GOP runtime positions should remain in native subpatch coordinates");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"range",atoms:["1","1000"]})&&applyIncomingGuiMessage({receiver:"live-level",selector:"log",atoms:[]}), "runtime logarithmic mode should reach GOP sliders");
const logarithmicGopChild=gopItemObject(liveGop,gopPreviewItems(liveGop).find(item=>item.lineIndex===liveGopItem.lineIndex));
logarithmicGopChild.value=Math.sqrt(1000);
assert(Math.abs(guiValueToNorm(logarithmicGopChild)-0.5)<0.0001, "runtime logarithmic ranges should use Pd-style exponential display mapping");
assert(applyIncomingGuiMessage({receiver:"live-level",selector:"lin",atoms:[]})&&gopPreviewItems(liveGop).find(item=>item.lineIndex===liveGopItem.lineIndex).runtimeState.log===false, "runtime lin messages should restore linear slider mapping");
const runtimeRadio={id:999,kind:"obj",x:0,y:0,text:"hradio 20 1 0 8 empty radio-in empty 0 -10 0 12 #fcfcfc #000000 #000000 0",sourceDisplayText:"",guiParts:["hradio","20","1","0","8","empty","radio-in","empty","0","-10","0","12","#fcfcfc","#000000","#000000","0"]},runtimeRadioState={};
assert(applyRuntimeGuiMessage(runtimeRadio,"number",["12"],runtimeRadioState)&&guiRadioCount(runtimeDisplayObject(runtimeRadio,runtimeRadioState))===12, "runtime radio number messages should change the visible cell count");
assert(applyRuntimeGuiMessage(runtimeRadio,"orientation",["1"],runtimeRadioState)&&guiBaseName(canonicalName(runtimeDisplayObject(runtimeRadio,runtimeRadioState)))==="vradio", "runtime orientation messages should rotate radio controls");
const runtimeBang={id:1000,kind:"obj",x:0,y:0,text:"bng 20 250 50 0 empty bang-in empty 0 -10 0 12 #fcfcfc #000000 #000000",sourceDisplayText:"",guiParts:["bng","20","250","50","0","empty","bang-in","empty","0","-10","0","12","#fcfcfc","#000000","#000000"]},runtimeBangState={};
assert(applyRuntimeGuiMessage(runtimeBang,"flashtime",["40","180"],runtimeBangState)&&runtimeBangState.flashBreak===40&&runtimeBangState.flashHold===180, "runtime bang flashtime messages should retain both native timing values");
assert(applyRuntimeGuiMessage(runtimeBang,"anything",["payload"],runtimeBangState), "bang controls should flash for arbitrary Pd messages");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"send",atoms:["runtime-output"]}), "runtime send messages should update the live IEM send slot");
assert(applyIncomingGuiMessage({receiver:"live-toggle",selector:"receive",atoms:["runtime-input"]}), "runtime receive messages should update the live IEM receive slot");
assert(!applyIncomingGuiMessage({receiver:"live-toggle",selector:"set",atoms:["0"]}), "an IEM control should stop responding on its previous runtime receive symbol");
assert(applyIncomingGuiMessage({receiver:"runtime-input",selector:"set",atoms:["0"]})&&runtimeDisplayObject(objects[0]).value===0, "an IEM control should respond on its replacement runtime receive symbol");
assert(monitoredGuiReceivers().includes("runtime-input")&&!monitoredGuiReceivers().includes("live-toggle"), "runtime receive changes should refresh editor monitoring routes");
assert(serialize()===livePropertySource, "live Pd GUI property messages must not rewrite serialized patch source");
parsePatch(fixture,false);
const dollarSubpatchFixture = "#N canvas 20 30 520 360 10;\\n"
 + "#N canvas 120 120 360 260 scoped 0;\\n"
 + "#X obj 20 20 r \\\\$0-local-bus;\\n"
 + "#X msg 20 60 \\\\$1 \\\\$2 \\\\$0-local-bus;\\n"
 + "#X text 20 100 scoped \\\\$0 note;\\n"
 + "#X restore 180 120 pd scoped patch \\\\$0;\\n"
 + "#X obj 20 20 s \\\\$0-root-bus;\\n";
parsePatch(dollarSubpatchFixture,false);
const dollarSubpatch = objects.find(o => o.sourceKind === "subpatch");
assert(dollarSubpatch && dollarSubpatch.subpatchRestoreParts.join(" ") === "pd scoped patch $0", "subpatch restore parts did not unescape dollar tokens for editing");
assert(serialize().includes("#X restore 180 120 pd scoped patch \\\\$0;"), "subpatch restore dollar token did not round-trip");
assert(enterSubpatchCanvas(objects.indexOf(dollarSubpatch)), "could not enter dollar-token subpatch");
objects.push({ id: nextId++, kind: "obj", x: 40, y: 140, text: "s $0-added-bus" });
leaveSubpatchCanvas();
const dollarOut = serialize();
assert(dollarOut.includes("#X obj 20 20 r \\\\$0-local-bus;"), "inner $0 receiver did not remain escaped after subpatch edit");
assert(dollarOut.includes("#X msg 20 60 \\\\$1 \\\\$2 \\\\$0-local-bus;"), "inner dollar message did not remain escaped after subpatch edit");
assert(dollarOut.includes("#X obj 40 140 s \\\\$0-added-bus;"), "new inner $0 sender was not escaped after leaving subpatch");
assert(dollarOut.includes("#X restore 180 120 pd scoped patch \\\\$0;"), "restore line dollar token was not preserved after leaving subpatch");
parsePatch(dollarSubpatchFixture,false);
const sourceEditedSubpatch = objects.find(o => o.sourceKind === "subpatch");
assert(enterSubpatchCanvas(objects.indexOf(sourceEditedSubpatch)), "could not enter source-edited subpatch");
toggleSource(document.getElementById("source"));
sourceText.value = serialize() + "\\n#X obj 70 170 s \\\\$0-source-added;";
leaveSubpatchCanvas();
assert(serialize().includes("#X obj 70 170 s \\\\$0-source-added;"), "leaving subpatch with global source open lost the source edit");
assert(!sourceVisible && editingSourceObject < 0, "leaving a subpatch should close transient source editor state");
parsePatch(dollarSubpatchFixture,false);
toggleSource(document.getElementById("source"));
sourceText.value = "#N canvas 20 30 420 220 10;\\n#X obj 20 20 osc~ 330;\\n";
assert(!enterSubpatchCanvas(0), "entering a subpatch should revalidate after committing global source text");
assert(canvasStack.length === 0 && objects.length === 1 && canonicalName(objects[0]) === "osc~", "failed stale subpatch navigation should leave the committed root patch open");
const nestedIdFixture = "#N canvas 20 30 520 360 10;\\n"
 + "#N canvas 120 120 260 180 child 0;\\n"
 + "#X obj 20 20 inlet;\\n"
 + "#X restore 180 120 pd child;\\n"
 + "#X obj 20 20 osc~ 220;\\n"
 + "#X obj 20 70 *~;\\n"
 + "#X obj 20 120 dac~;\\n";
parsePatch(nestedIdFixture,false);
const parentNextBeforeNested = nextId;
const parentIdsBeforeNested = new Set(objects.map(o => o.id));
const childObject = objects.find(o => o.sourceKind === "subpatch");
assert(enterSubpatchCanvas(objects.indexOf(childObject)), "could not enter child subpatch for id preservation test");
leaveSubpatchCanvas();
addObject("obj",240,180,"print after-child");
assert(objects[objects.length - 1].id === parentNextBeforeNested, "leaving a child canvas did not restore the parent next object id");
assert(!parentIdsBeforeNested.has(objects[objects.length - 1].id), "new parent object reused an existing parent id after leaving a child canvas");
backendEvents.length = 0;
registerHelpPatchSources({
 "osc~": "#N canvas 10 10 320 220 10;\\n#X text 20 20 oscillator help;\\n",
 "delay": "#N canvas 10 10 320 220 10;\\n#X text 20 20 delay help;\\n",
 "trigger": "#N canvas 10 10 320 220 10;\\n#X text 20 20 trigger help;\\n",
 "hradio": "#N canvas 10 10 320 220 10;\\n#X text 20 20 radio help;\\n",
 "sqrt~": "#N canvas 10 10 320 220 10;\\n#X text 20 20 signal sqrt help;\\n",
 "rsqrt~": "#N canvas 10 10 320 220 10;\\n#X text 20 20 reciprocal sqrt help;\\n"
});
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 osc~ 220;\\n",false);
const helpRootPatch = serialize();
selectOnly(0);
assert(openHelpForSelected(), "registered help patch did not open for selected object");
objects.push({ id: nextId++, kind: "obj", x: 40, y: 80, text: "print help-edit" });
leaveSubpatchCanvas();
assert(serialize() === helpRootPatch, "leaving a help patch should not mutate the parent patch");
assert(!backendEvents.some(event => event.name === "patchChanged"), "leaving a help patch should not emit a parent patch change");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 osc~ 220;\\n",false);
selectOnly(0);
toggleSource(document.getElementById("source"));
sourceText.value = "#N canvas 20 30 360 220 10;\\n#X obj 20 20 unknown-no-help;\\n";
assert(!openHelpForSelected(), "opening help should revalidate after committing global source text");
assert(canvasStack.length === 0 && canonicalName(objects[0]) === "unknown-no-help", "failed stale help navigation should leave the committed root patch open");
parsePatch("#N canvas 20 30 360 220 10;\\n#X obj 20 20 del 50;\\n#X obj 20 70 t b f;\\n#X obj 20 120 radiobutton;\\n#X obj 20 170 q8_sqrt~;\\n#X obj 20 220 q8_rsqrt~;\\n",false);
assert(helpNameForObject(objects[0]) === "delay", "del should resolve to delay help");
assert(helpNameForObject(objects[1]) === "trigger", "t should resolve to trigger help");
assert(helpNameForObject(objects[2]) === "hradio", "radiobutton should resolve to hradio help");
assert(helpNameForObject(objects[3]) === "sqrt~", "q8_sqrt~ should resolve to sqrt~ help");
assert(helpNameForObject(objects[4]) === "rsqrt~", "q8_rsqrt~ should resolve to rsqrt~ help");
backendEvents.length = 0;
registerAbstractionPorts({ "voice": [2, 1] });
registerAbstractionSources({
 "voice": "#N canvas 120 120 360 260 voice 0;\\n"
  + "#X obj 20 20 inlet;\\n"
  + "#X obj 20 60 r \\\\$0-voice-bus;\\n"
  + "#X msg 20 100 \\\\$1 \\\\$0-voice-bus;\\n"
  + "#X obj 20 140 outlet;\\n"
});
parsePatch("#N canvas 20 30 420 260 10;\\n#X obj 80 80 voice \\\\$0-instance;\\n",false);
const voiceObject = objects.find(o => canonicalName(o) === "voice");
assert(voiceObject && isKnownObject(voiceObject) && objectArity(voiceObject)[0] === 2 && objectArity(voiceObject)[1] === 1, "registered abstraction did not expose known ports");
assert(enterAbstractionCanvas(objects.indexOf(voiceObject)), "could not enter abstraction source");
objects.push({ id: nextId++, kind: "obj", x: 64, y: 180, text: "s $0-added-voice" });
leaveSubpatchCanvas();
const updatedVoice = localAbstractionSources.get("voice") || "";
assert(updatedVoice.includes("#X obj 20 60 r \\\\$0-voice-bus;"), "abstraction $0 receiver did not remain escaped");
assert(updatedVoice.includes("#X msg 20 100 \\\\$1 \\\\$0-voice-bus;"), "abstraction dollar message did not remain escaped");
assert(updatedVoice.includes("#X obj 64 180 s \\\\$0-added-voice;"), "new abstraction $0 sender was not escaped");
assert(serialize().includes("#X obj 80 80 voice \\\\$0-instance;"), "root abstraction object did not preserve its dollar argument");
assert(backendEvents.some(event => event.name === "abstractionChanged" && event.payload && event.payload.name === "voice" && String(event.payload.patch || "").includes("\\\\$0-added-voice")), "abstraction edit did not emit updated source");
assert(enterAbstractionCanvas(objects.indexOf(voiceObject)), "could not re-enter abstraction source");
toggleSource(document.getElementById("source"));
sourceText.value = serialize() + "\\n#X obj 92 212 s \\\\$0-source-added-voice;";
leaveSubpatchCanvas();
assert((localAbstractionSources.get("voice") || "").includes("#X obj 92 212 s \\\\$0-source-added-voice;"), "leaving abstraction with global source open lost the source edit");
parsePatch("#N canvas 20 30 420 260 10;\\n#X obj 80 80 voice \\\\$0-instance;\\n",false);
toggleSource(document.getElementById("source"));
sourceText.value = "#N canvas 20 30 420 260 10;\\n#X obj 80 80 not_registered;\\n";
assert(!enterAbstractionCanvas(0), "entering an abstraction should revalidate after committing global source text");
assert(canvasStack.length === 0 && canonicalName(objects[0]) === "not_registered", "failed stale abstraction navigation should leave the committed root patch open");
const outletFixture = "#N canvas 20 30 360 220 10;\\n"
 + "#X obj 20 20 bng 20 250 50 0 empty empty fire 0 -10 0 12 #fcfcfc #000000 #000000;\\n"
 + "#X msg 20 70 bang;\\n"
 + "#X connect 0 0 1 0;\\n";
parsePatch(outletFixture,false);
const savedOutlet = serialize();
const outletPatch = patchWithGuiOutletReceiver(0,"otherware_outlet_test");
assert(savedOutlet.indexOf("otherware_outlet_test") < 0, "temporary GUI outlet receiver leaked into saved patch");
assert(outletPatch.includes("r otherware_outlet_test"), "temporary GUI outlet receiver was not added");
assert(outletPatch.includes("#X connect 2 0 1 0;"), "temporary GUI outlet receiver was not connected to GUI downstream objects");
backendEvents.length = 0;
emitGuiTrigger(0);
const unsentGuiEvent = backendEvents.find(event => event.name === "guiTriggered");
assert(unsentGuiEvent && unsentGuiEvent.payload.patch.indexOf("otherware_gui_") < 0, "GUI trigger leaked its temporary receiver into the saved patch payload");
assert(unsentGuiEvent.payload.triggerPatch.indexOf("otherware_gui_") >= 0&&unsentGuiEvent.payload.triggerPatch.includes("#X connect 2 0 0 0;"), "GUI trigger did not enter through the control's inlet in its playback-only patch");
parsePatch("#N canvas 20 30 520 360 10;\\n"
 + "#X array trigger_graph 8 float 0;\\n"
 + "#A 0 0 1 0 -1 0 1 0 -1;\\n"
 + "#X coords 0 1 7 -1 200 140 1 0 0;\\n"
 + "#X restore 220 160 graph;\\n"
 + "#X msg 20 20 bang;\\n",false);
const graphSavedPatch = serialize();
const graphTriggerPatch = patchWithObjectBangReceiver(1,"otherware_bang_graph");
assert(graphSavedPatch.includes("#X restore 220 160 graph;"), "saved graph-array patch lost its restore wrapper");
assert(graphTriggerPatch.includes("#X restore 220 160 graph;"), "temporary trigger patch should preserve graph-array wrapper metadata");
backendEvents.length = 0;
parsePatch("#N canvas 20 30 360 220 10;\\n"
 + "#X msg 20 20 440 1;\\n"
 + "#X obj 20 70 unpack f f;\\n"
 + "#X connect 0 0 1 0;\\n",false);
const savedMessagePatch = serialize();
assert(emitObjectBangTrigger(0), "message boxes should produce a playback trigger in run mode");
const messageTriggerEvent = backendEvents.find(event => event.name === "guiTriggered");
assert(messageTriggerEvent && messageTriggerEvent.payload.selector === "bang", "message box trigger should send a bang into the temporary receiver");
assert(messageTriggerEvent.payload.patch === savedMessagePatch, "message box trigger changed the saved patch payload");
assert(messageTriggerEvent.payload.triggerPatch.includes("r otherware_bang_"), "message box trigger did not include a temporary receiver");
assert(messageTriggerEvent.payload.triggerPatch.includes("#X connect 2 0 0 0;"), "message box temporary receiver was not connected to the message inlet");
backendEvents.length = 0;
parsePatch("#N canvas 20 30 520 260 10;\\n"
 + "#X msg 20 20 \\\\; target-a 1 \\\\; target-b symbol go, f 32;\\n"
 + "#X obj 20 90 r target-a;\\n"
 + "#X obj 180 90 r target-b;\\n",false);
const savedSemicolonClickPatch = serialize();
assert(emitObjectBangTrigger(0), "semicolon message boxes should also produce a playback trigger in run mode");
const semicolonClickEvent = backendEvents.find(event => event.name === "guiTriggered");
assert(semicolonClickEvent && semicolonClickEvent.payload.patch === savedSemicolonClickPatch, "semicolon message trigger changed the saved patch payload");
assert(semicolonClickEvent.payload.patch.includes("\\\\; target-a 1 \\\\; target-b symbol go"), "semicolon message text was not preserved in saved trigger payload");
assert(semicolonClickEvent.payload.patch.indexOf("otherware_bang_") < 0, "semicolon message trigger leaked the temporary receiver into the saved patch");
assert(semicolonClickEvent.payload.triggerPatch.includes("r otherware_bang_"), "semicolon message trigger did not include a playback-only receiver");
assert(semicolonClickEvent.payload.triggerPatch.includes("#X connect 3 0 0 0;"), "semicolon message temporary receiver was not connected to the message inlet");
parsePatch("#N canvas 20 30 420 260 10;\\n#X obj 20 20 f 123;\\n#X obj 220 20 r passive;\\n",false);
assert(!canBangObjectBox(0) && !emitObjectBangTrigger(0), "ordinary Pd object boxes should not fake run-mode clicks");
assert(!canBangObjectBox(1), "receive-only objects should not be fake trigger targets");
const helpCorpus = ${JSON.stringify(fs.existsSync("third_party/pure-data/extra")
  ? fs.readdirSync("third_party/pure-data/extra", { recursive: true })
      .filter(name => String(name).endsWith("-help.pd"))
      .map(name => String(name))
      .sort()
  : [])};
const bundledPdCorpus = ${JSON.stringify(fs.existsSync("third_party/pure-data/extra")
  ? fs.readdirSync("third_party/pure-data/extra", { recursive: true })
      .filter(name => String(name).endsWith(".pd"))
      .map(name => String(name))
      .sort()
  : [])};
const externalPdCorpus = ${JSON.stringify(externalPdCorpus)};
const externalPdCorpusRoot = ${JSON.stringify(externalPdCorpusRoot)};
function assertPdCorpusRoundTrip(name, label, root=${JSON.stringify(helpPatchDir)}) {
 const source = require("fs").readFileSync(root + "/" + name, "utf8");
 const sourceRecords=pdSourceRecords(source);
 let sourceDepth=0,sourceObjectCount=0,sourceConnectionCount=0,seenMainCanvas=false;
 sourceRecords.forEach(line=>{
  if(line.startsWith("#N canvas ")){sourceDepth++;seenMainCanvas=true;return;}
  if(!seenMainCanvas)return;
  if(line.startsWith("#X restore ")){if(sourceDepth===2)sourceObjectCount++;sourceDepth=Math.max(1,sourceDepth-1);return;}
  if(sourceDepth!==1)return;
  if(/^#X (obj|msg|text|floatatom|symbolatom|listbox|scalar|array)(?: |;)/.test(line))sourceObjectCount++;
  if(line.startsWith("#X connect "))sourceConnectionCount++;
 });
 parsePatch(source,false);
 const objectCount = objects.length;
 const connectionCount = connections.length;
 assert(objectCount===sourceObjectCount,label+" top-level object model omitted or invented records: "+name+" expected "+sourceObjectCount+" got "+objectCount);
 assert(connectionCount===sourceConnectionCount,label+" top-level cord model omitted or invented records: "+name+" expected "+sourceConnectionCount+" got "+connectionCount);
 const importantLines = source.split(/\\r?\\n/).map(line => line.trim()).filter(line =>
  line.startsWith("#X declare ") || line.startsWith("#X coords ") || line.startsWith("#X array ") ||
  line.startsWith("#X graph ") || line.startsWith("#X restore ") || line.startsWith("#X connect ") || line.startsWith("#X f ") ||
  line.startsWith("#A "));
 const importantLineCounts = new Map();
 importantLines.forEach(line => importantLineCounts.set(line, (importantLineCounts.get(line) || 0) + 1));
 const serialized = serialize();
 parsePatch(serialized,false);
 assert(objects.length === objectCount, label + " object count drifted after round-trip: " + name);
 assert(connections.length === connectionCount, label + " connection count drifted after round-trip: " + name);
 assert(pdSourceRecords(serialize()).some(line=>line.startsWith("#N canvas ")), label + " stopped serializing as a Pd canvas: " + name);
 const sourcePreamble=pdSourceRecords(source).filter((line,index,all)=>index<all.findIndex(item=>item.startsWith("#N canvas ")));
 assert(JSON.stringify(canvasPreamble)===JSON.stringify(sourcePreamble), label + " changed records preceding the main canvas: " + name);
 const stableSerialized=serialize();
 assert(stableSerialized===serialized, label + " did not reach a stable editor serialization after one round-trip: " + name);
 importantLineCounts.forEach((expectedCount, line) => {
  const actualCount = serialized.split(line).length - 1;
  assert(actualCount === expectedCount, label + " metadata line count drifted after round-trip: " + name + " :: " + line + " expected " + expectedCount + " got " + actualCount);
 });
}
const multilineLegacyPatch="#N canvas 10 10 420 260 12;\\n#X text 20 20 a legacy text record whose\\ncontinuation remains part of the same Pd record\\nuntil its unescaped terminator;\\n#X obj 20 90 osc~ 220;\\n";
assert(pdSourceRecords(multilineLegacyPatch).length===3, "Pd record tokenizer should join physical lines until an unescaped semicolon");
parsePatch(multilineLegacyPatch,false);
assert(objects.length===2&&objects[0].kind==="comment"&&objects[0].text.includes("continuation remains part"), "multiline legacy Pd text records should parse as one comment object");
const standaloneWidthPatch="#N canvas 10 10 420 260 12;\\n#N canvas 20 20 180 120 child 0;\\n#X obj 20 20 inlet;\\n#X restore 40 50 pd child;\\n#X f 17;\\n";
parsePatch(standaloneWidthPatch,false);
assert(objects.length===1&&objects[0].standaloneFormatHints[0]==="#X f 17;"&&formatHintChars(objects[0])===17, "standalone Pd width records should remain attached to restored subpatch boxes");
assert(serialize().includes("#X restore 40 50 pd child;\\n#X f 17;"), "standalone Pd width records should serialize immediately after their owning object");
const graphArrayPatch="#N canvas 10 10 620 420 12;\\n#N canvas 0 0 450 300 (subpatch) 0;\\n#X array graph-array 5 float 2;\\n#A 0 0 0.5 1 -0.5 -1;\\n#A color 5;\\n#A width 3;\\n#X coords 0 1 5 -1 300 150 1 0 0;\\n#X xlabel -0.8 0 1 2 3 4;\\n#X ylabel -0.2 -1 0 1;\\n#X restore 40 80 graph;\\n";
parsePatch(graphArrayPatch,false);
const graphArrayParent=objects[0],graphArrayItem=gopPreviewItems(graphArrayParent).find(item=>item.kind==="array"),graphArrayChild=graphArrayItem&&gopItemObject(graphArrayParent,graphArrayItem);
assert(graphArrayParent&&graphArrayParent.subpatchGraphOnParent&&graphArrayItem&&graphArrayChild, "graph-on-parent array subcanvases should expose their array to the parent renderer");
assert(monitoredArrayNames().join(",")==="graph-array", "visible graph arrays should be registered for live libpd snapshots");
const graphArraySavedBeforeLive=serialize();
applyIncomingArraySnapshots({arrays:[{name:"graph-array",totalSize:5,values:[0.25,0.5,0.75,1,-1]}]});
assert(arrayValues(graphArrayChild).join(",")==="0.25,0.5,0.75,1,-1", "live libpd samples should replace the displayed graph-array values");
assert(serialize()===graphArraySavedBeforeLive, "live libpd array snapshots must not contaminate saved Pd source");
applyIncomingArraySnapshots({arrays:[]});
assert(arrayValues(graphArrayChild).join(",")==="0,0.5,1,-0.5,-1", "array color and width metadata must not be mistaken for sample data");
assert(arrayVisualStyle(graphArrayChild).color==="#ff0400"&&arrayVisualStyle(graphArrayChild).width===3, "graph arrays should retain native Pd color and line-width metadata");
assert(arrayGraphCoordinates(graphArrayChild).w===300&&arrayGraphCoordinates(graphArrayChild).h===150&&graphArrayChild.arrayTrailingLines.some(line=>line.startsWith("#X xlabel "))&&graphArrayChild.arrayTrailingLines.some(line=>line.startsWith("#X ylabel ")), "graph arrays should retain native dimensions and axis labels");
render();
assert(svg.children.some(node=>node.classList&&node.classList.contains("box")), "graph array patch should render its parent canvas object");
const graphArrayPlot=arrayPlotRect({...graphArrayChild,x:graphArrayParent.x,y:graphArrayParent.y},boxSize(graphArrayParent));
const graphArrayStart={x:graphArrayPlot.left,y:graphArrayPlot.top},graphArrayEnd={x:graphArrayPlot.right,y:graphArrayPlot.bottom};
const graphArrayHit=gopArrayAt(0,graphArrayStart);
assert(graphArrayHit&&beginGopArrayDraw(graphArrayHit,graphArrayStart), "run-mode graph arrays should begin drawing from their visible parent plot");
assert(continueArrayDraw(graphArrayEnd), "graph array drawing should interpolate continuously across samples");
const editedGraphArrayItem=gopPreviewItems(graphArrayParent).find(item=>item.kind==="array"),editedGraphArray=editedGraphArrayItem.child;
assert(arrayValues(editedGraphArray)[0]===1&&arrayValues(editedGraphArray)[4]===-1, "graph array strokes should map parent-canvas coordinates through the native graph range");
assert(arrayValues(editedGraphArray)[2]===0, "graph array strokes should interpolate through intermediate samples");
assert(editedGraphArray.arrayDataLines.some(line=>line==="#A color 5;")&&editedGraphArray.arrayDataLines.some(line=>line==="#A width 3;"), "drawing graph arrays must preserve color and width metadata");
assert(graphArrayParent.subpatchText.includes("#X xlabel -0.8")&&graphArrayParent.subpatchText.includes("#X ylabel -0.2"), "drawing graph arrays must preserve axis metadata");
arrayDrag=null;
const rangedAtom={id:1001,kind:"obj",x:100,y:100,text:"floatatom",sourceKind:"floatatom",sourceArgs:"5 -2 2 0 level empty empty 12",value:0};
assert(atomDragRange(rangedAtom).low===-2&&atomDragRange(rangedAtom).high===2, "float atoms should expose their native saved drag range");
assert(updateGuiObject(rangedAtom,{x:100,y:80},{start:{x:100,y:100},startValue:0,shiftKey:false,altKey:false})&&rangedAtom.value===2, "normal float-atom dragging should use one unit per vertical canvas unit and clip to its range");
rangedAtom.value=0;
assert(updateGuiObject(rangedAtom,{x:100,y:-200},{start:{x:100,y:100},startValue:0,shiftKey:true,altKey:false})&&rangedAtom.value===0.5, "Shift-dragging float atoms should use Pd's hundredth-unit precision");
let atomLabel=guiLabelInfo(rangedAtom);
assert(atomLabel&&atomLabel.text==="level"&&atomLabel.x<0, "float atom label flag 0 should place its label on the left");
rangedAtom.sourceArgs="5 -2 2 1 level empty empty 12";atomLabel=guiLabelInfo(rangedAtom);
assert(atomLabel&&atomLabel.x>atomBoxSize(rangedAtom).w, "float atom label flag 1 should place its label on the right");
rangedAtom.sourceArgs="5 -2 2 2 level empty empty 12";atomLabel=guiLabelInfo(rangedAtom);
assert(atomLabel&&atomLabel.y<0, "float atom label flag 2 should place its label above");
rangedAtom.sourceArgs="5 -2 2 3 level empty empty 12";atomLabel=guiLabelInfo(rangedAtom);
assert(atomLabel&&atomLabel.y>atomBoxSize(rangedAtom).h, "float atom label flag 3 should place its label below");
const numericListAtom={id:1002,kind:"obj",x:40,y:40,text:"listbox",sourceKind:"listbox",sourceArgs:"20 0 0 0 values empty empty 12",symbolValue:"pitch 60 0.5"};
const secondTokenX=numericListAtom.x+8+textWidth("pitch ")+textWidth("60")*.5,listDragState={start:{x:secondTokenX,y:40},shiftKey:false};
assert(canDragGuiObject(numericListAtom)&&listAtomIndexAt(numericListAtom,listDragState.start)===1, "list boxes should expose numeric atoms under the pointer for native dragging");
assert(updateGuiObject(numericListAtom,{x:secondTokenX,y:16},listDragState)&&numericListAtom.symbolValue==="pitch 64 0.5", "dragging a numeric list-box atom should update only that atom");
const thirdTokenX=numericListAtom.x+8+textWidth("pitch 64 ")+textWidth("0.5")*.5,listFineState={start:{x:thirdTokenX,y:40},shiftKey:true};
assert(updateGuiObject(numericListAtom,{x:thirdTokenX,y:-20},listFineState)&&numericListAtom.symbolValue==="pitch 64 0.6", "Shift-dragging numeric list-box atoms should use hundredth-unit precision");
assert(listAtomIndexAt(numericListAtom,{x:numericListAtom.x+10,y:40})===-1, "symbol atoms inside list boxes should remain text rather than becoming fake numeric drags");
objects=[rangedAtom];connections=[];extraLines=[];postConnectionLines=[];canvasPreamble=[];canvasHeader="#N canvas 10 10 420 260 12;";setEditMode(true);
assert(openPropertiesForObject(0), "native atom boxes should open the properties panel");
assert(document.getElementById("propWidth").value==="5"&&document.getElementById("propMin").value==="-2"&&document.getElementById("propMax").value==="2", "float atom properties should expose width and drag range");
assert(applyGuiProperties(0,{width:"12",min:"-24",max:"24",label:"pitch value",labelX:"1",receive:"pitch-in",send:"pitch-out",fontSize:"16"}), "native atom properties should apply through the shared properties workflow");
assert(objects[0].sourceArgs==="12 -24 24 1 pitch\\\\ value pitch-in pitch-out 16", "atom properties should preserve Pd escaping and native field order");
assert(serialize().includes("#X floatatom 100 100 12 -24 24 1 pitch\\\\ value pitch-in pitch-out 16;"), "atom properties should serialize as native Pd atom syntax");
closeProperties();
assert(helpCorpus.length >= 12, "bundled Pd help patch corpus was not found");
helpCorpus.forEach(name => assertPdCorpusRoundTrip(name, "help patch"));
assert(bundledPdCorpus.length >= helpCorpus.length, "bundled Pd patch corpus was not found");
bundledPdCorpus.forEach(name => assertPdCorpusRoundTrip(name, "bundled Pd patch"));
externalPdCorpus.forEach(name => assertPdCorpusRoundTrip(name, "vanilla Pd documentation patch", externalPdCorpusRoot));
console.log("pd editor round-trip checks passed");
`;

context.require = require;
vm.runInNewContext(match[1] + "\n" + tests, context, { filename: htmlPath });
