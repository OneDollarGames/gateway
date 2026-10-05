# -*- coding: utf-8 -*-
"""
Compilador de sesiones de Gateway.

Aqui viven los guiones (en espanol) y la estructura de cada sesion: segmentos con voz,
silencios, receta Hemi-Sync (portadoras/batidos medidos en las cintas y en las patentes de
Monroe), visual del domo, flicker y respiracion guiada. El script:

  1. genera la voz de cada segmento con OpenAI TTS (voz nova, estilo hipnotico) y la guarda en
     Content/Gateway/Voice/<sesion>/NN_<hash>.wav (cache: solo se sintetiza lo nuevo);
  2. mide la duracion real de cada voz y calcula los tiempos de la sesion;
  3. escribe Content/Gateway/Sessions/<orden>_<sesion>.json, que es lo que lee el juego.

Uso:  python3 Tools/sesiones.py            (genera todo; necesita OPENAI_API_KEY)
      python3 Tools/sesiones.py --dry      (solo JSON, duraciones estimadas, sin TTS)
      python3 Tools/sesiones.py --solo f10 (una sesion)
"""
import hashlib
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VOICE_DIR = os.path.join(ROOT, "Content", "Gateway", "Voice")
SESS_DIR = os.path.join(ROOT, "Content", "Gateway", "Sessions")
DRY = "--dry" in sys.argv
SOLO = None
if "--solo" in sys.argv:
    SOLO = sys.argv[sys.argv.index("--solo") + 1]

TTS_MODEL = "gpt-4o-mini-tts"
TTS_VOICE = "nova"
TTS_INSTRUCTIONS = (
    "Narradora de meditación guiada en español neutro latinoamericano. Habla EXTREMADAMENTE despacio, mucho más lento "
    "que una conversación normal: cada palabra se pronuncia completa y sin prisa, con voz grave, cálida, serena y segura, "
    "casi en susurro pero clara. Haz pausas largas, de dos o tres segundos, en los puntos suspensivos y entre frases. "
    "Hipnótica, íntima, como si hablaras a alguien que está a punto de dormirse. "
    "Los números de los conteos se dicen muy lentos, uno por uno, con silencio entre ellos."
)
# Estiramiento temporal adicional (ffmpeg atempo, conserva el tono): 0.88 = 12 % mas lento
VOZ_TEMPO = 0.88

# ----------------------------------------------------------------------------- recetas de sonido
# capas: (portadora, batido, gain, iso, pan)

def capas(*lst):
    out = []
    for c in lst:
        d = {"portadora": c[0], "batido": c[1], "gain": c[2]}
        if len(c) > 3 and c[3]:
            d["iso"] = c[3]
        if len(c) > 4 and c[4]:
            d["pan"] = c[4]
        out.append(d)
    return out

SONIDO = {
    # Silencio total (solo voz)
    "nada": {"capas": [], "rosa": 0.0},
    # Oleaje de fondo mientras el cuerpo se acomoda (la cinta original usa surf)
    "oleaje": {"capas": capas((100, 1.5, 0.08)), "marron": 0.25, "oceano": 0.11},
    # Preparacion: 50[0.5] + 100[1.5] medidos en la intro de las cintas
    "prep": {"capas": capas((50, 0.5, 0.14), (100, 1.5, 0.18)), "rosa": 0.08, "marron": 0.12, "oceano": 0.1},
    # Focus 3: 100[1.3] + 288[3.7]
    "f3": {"capas": capas((100, 1.3, 0.2), (288, 3.7, 0.18)), "rosa": 0.08},
    # Focus 10: 100[1.5] 200[4] 250[4] 300[4] (+ un toque isocronico en la de 200, que entrena mejor el EEG)
    "f10": {"capas": capas((100, 1.5, 0.22), (200, 4.0, 0.2, 0.3), (250, 4.0, 0.16), (300, 4.0, 0.14)), "rosa": 0.07},
    # Focus 10 profundo para dormir
    "f10_sueno": {"capas": capas((100, 1.5, 0.2), (200, 4.0, 0.16), (300, 4.0, 0.12)), "rosa": 0.05, "marron": 0.1, "oceano": 0.07},
    # Focus 12: F10 + 400[10] 500[10.1] 600[4.8] (+50[0.25])
    "f12": {"capas": capas((50, 0.25, 0.1), (100, 1.5, 0.2), (200, 4.0, 0.18, 0.25), (250, 4.0, 0.14), (300, 4.0, 0.12), (400, 10.0, 0.12), (500, 10.1, 0.1), (600, 4.8, 0.08)), "rosa": 0.06},
    # Focus 15: F10 + 500[7.05] 630[7.1] 750[7.0]
    "f15": {"capas": capas((100, 1.5, 0.2), (200, 4.0, 0.16), (250, 4.0, 0.12), (300, 4.0, 0.1), (500, 7.05, 0.12), (630, 7.1, 0.1), (750, 7.0, 0.08)), "rosa": 0.05},
    # Focus 21: 200[4] 250[4] 300[4] + 600[16.2] 750[15.9] 900[16.2]
    "f21": {"capas": capas((200, 4.0, 0.18), (250, 4.0, 0.14), (300, 4.0, 0.12), (600, 16.2, 0.12), (750, 15.9, 0.1), (900, 16.2, 0.08)), "rosa": 0.04},
    # Procesador de sueno de la patente 5,356,368: theta -> delta -> delta bajo
    "sueno_theta": {"capas": capas((100, 4.0, 0.18), (300, 4.0, 0.12), (500, 2.0, 0.08)), "marron": 0.14, "oceano": 0.06},
    "sueno_delta": {"capas": capas((100, 1.5, 0.16), (300, 1.0, 0.1), (496, 1.0, 0.06)), "marron": 0.12, "oceano": 0.05},
    "sueno_profundo": {"capas": capas((100, 0.75, 0.12), (300, 0.5, 0.08)), "marron": 0.1, "oceano": 0.04},
    # Senal de retorno a C-1: beta 14-15 Hz (308/322 y 500/515 medidos)
    "retorno": {"capas": capas((308, 14.0, 0.22), (500, 15.0, 0.16), (200, 4.0, 0.06)), "rosa": 0.03},
    # Ciencia: theta 6 Hz sobre 250 Hz (Jirakittayakorn 2017) con componente isocronica
    "theta6": {"capas": capas((250, 6.0, 0.26, 0.45), (100, 1.5, 0.12)), "rosa": 0.08},
    # Ciencia: alfa 10 Hz sobre 250 Hz para acompanar el flicker a 10 Hz
    "alfa10": {"capas": capas((250, 10.0, 0.24, 0.5), (400, 10.0, 0.12)), "rosa": 0.1},
    # Demostracion Hemi-Sync de la Orientacion: solo izquierda / solo derecha / ambos
    "demo_izq": {"capas": capas((300, 0.0, 0.25, 0, -1)), "rosa": 0.0},
    "demo_der": {"capas": capas((304, 0.0, 0.25, 0, 1)), "rosa": 0.0},
    "demo_ambos": {"capas": capas((302, 4.0, 0.25)), "rosa": 0.0},
    # Respiracion en resonancia: tono que sigue la respiracion + theta suave
    "respira": {"capas": capas((250, 6.0, 0.14, 0.2)), "rosa": 0.06, "tono_respiracion": 0.5},
}

# ----------------------------------------------------------------------------- visuales del domo

VISUAL = {
    # Cosmos tenues: galaxia/anillo en indigo suave, muy lentos (los Pilares de la Creacion resultaban incomodos)
    "cosmos": {"modo": "cosmos", "color": [0.10, 0.14, 0.38], "intensidad": 0.15, "velocidad": 0.03, "complejidad": 0.5},
    "cosmos_lento": {"modo": "cosmos", "color": [0.08, 0.10, 0.30], "intensidad": 0.11, "velocidad": 0.02, "complejidad": 0.5},
    "galaxia": {"modo": "cosmos", "color": [0.18, 0.14, 0.42], "intensidad": 0.18, "velocidad": 0.03, "complejidad": 0.5},
    "anillo": {"modo": "cosmos", "color": [0.12, 0.22, 0.42], "intensidad": 0.16, "velocidad": 0.025, "complejidad": 0.5},
    "ganz_azul": {"modo": "ganzfeld", "color": [0.05, 0.12, 0.45], "intensidad": 0.5, "velocidad": 0.2},
    "ganz_violeta": {"modo": "ganzfeld", "color": [0.25, 0.05, 0.45], "intensidad": 0.5, "velocidad": 0.2},
    "ganz_rojo": {"modo": "ganzfeld", "color": [0.9, 0.22, 0.04], "intensidad": 0.6, "velocidad": 0.15},
    "ganz_naranja": {"modo": "ganzfeld", "color": [0.95, 0.45, 0.08], "intensidad": 0.55, "velocidad": 0.15},
    "ganz_verde": {"modo": "ganzfeld", "color": [0.05, 0.55, 0.2], "intensidad": 0.5, "velocidad": 0.15},
    "ganz_purpura": {"modo": "ganzfeld", "color": [0.45, 0.08, 0.6], "intensidad": 0.55, "velocidad": 0.15},
    "ganz_oscuro": {"modo": "ganzfeld", "color": [0.03, 0.02, 0.08], "intensidad": 0.4, "velocidad": 0.1},
    "orbe": {"modo": "orbe", "color": [0.45, 0.3, 1.0], "intensidad": 0.6, "velocidad": 0.35, "complejidad": 0.5},
    "orbe_dorado": {"modo": "orbe", "color": [1.0, 0.75, 0.3], "intensidad": 0.6, "velocidad": 0.3},
    "orbe_blanco": {"modo": "orbe", "color": [0.85, 0.9, 1.0], "intensidad": 0.7, "velocidad": 0.4},
    "tunel": {"modo": "tunel", "color": [0.2, 0.3, 1.0], "intensidad": 0.55, "velocidad": 0.35, "complejidad": 0.4},
    "tunel_lento": {"modo": "tunel", "color": [0.3, 0.15, 0.8], "intensidad": 0.45, "velocidad": 0.18, "complejidad": 0.3},
    "tunel_fuego": {"modo": "tunel", "color": [1.0, 0.45, 0.15], "intensidad": 0.55, "velocidad": 0.45, "complejidad": 0.5},
    "mandala": {"modo": "mandala", "color": [0.6, 0.3, 1.0], "intensidad": 0.6, "velocidad": 0.25, "complejidad": 0.55, "matiz": 0.25},
    "mandala_lento": {"modo": "mandala", "color": [0.3, 0.5, 1.0], "intensidad": 0.45, "velocidad": 0.12, "complejidad": 0.4, "matiz": 0.1},
    "vacio": {"modo": "vacio", "color": [0.3, 0.4, 0.9], "intensidad": 0.5, "velocidad": 0.1},
    "negro": {"modo": "vacio", "color": [0.0, 0.0, 0.0], "intensidad": 0.0, "velocidad": 0.0},
    "caja": {"modo": "caja", "color": [1.0, 0.7, 0.3], "intensidad": 0.6, "velocidad": 0.3, "imagen": "T_Caja01"},
}

FLICKER_OFF = {"hz": 0, "profundidad": 0}
def flicker(hz, prof, forma=0, color=(1.0, 0.95, 0.85)):
    return {"hz": hz, "profundidad": prof, "forma": forma, "color": list(color)}

def respira(rpm=6.0, inhalar=0.42, guia=True):
    return {"activa": True, "rpm": rpm, "inhalar": inhalar, "guia": guia}

# ----------------------------------------------------------------------------- segmentos

def seg(nombre, texto=None, pausa=0.0, sonido="f10", visual="orbe", flick=None, resp=None, campana=False, rampa=12.0, pantalla=None, voz_gain=0.6, campana_hz=528.0, campana_gain=0.35, duracion=None):
    """Un segmento: la voz (si hay) suena al inicio; `pausa` son los segundos de silencio tras la voz.
    `duracion` fija la duracion total (ignora la voz)."""
    return {"nombre": nombre, "texto": texto, "pausa": pausa, "sonido": sonido, "visual": visual, "flicker": flick, "resp": resp,
            "campana": campana, "rampa": rampa, "pantalla": pantalla, "voz_gain": voz_gain, "campana_hz": campana_hz, "campana_gain": campana_gain, "duracion": duracion}

# Textos reutilizados --------------------------------------------------------------------------

T_CANALES = ("Bienvenido a Gateway... Ponte cómodo. Debes escuchar mi voz en ambos oídos, y sentir que viene del centro de tu cabeza... "
             "Si no es así, revisa que los audífonos estén bien colocados... Ahora vas a oír un tono en el oído izquierdo... luego en el derecho... "
             "y después en los dos a la vez. Ajusta el volumen para que todo sea suave, apenas audible.")

T_CAJA = ("Ahora, crea en tu mente una caja... Una caja grande y fuerte, con una tapa pesada... tan fuerte que contiene cualquier cosa que pongas dentro... "
          "Es tu caja de conversión de energía... Levanta la tapa... y coloca dentro, por ahora, todas tus preocupaciones... tus pendientes... tus ansiedades... "
          "la materia física que te rodea... No las vas a necesitar aquí, y solo estorban... Tómate tu tiempo. Yo espero...")
T_CAJA_CIERRA = "Ahora cierra la tapa... con firmeza... y date la vuelta, dejando la caja atrás... Si algo te distrae durante el ejercicio, vuelve a la caja, ábrela, mete la distracción y ciérrala otra vez."

T_AFIRMACION = ("Repite mentalmente conmigo, despacio, la afirmación... Soy más que mi cuerpo físico... Porque soy más que materia física... "
                "deseo profundamente expandir... experimentar... conocer... comprender... controlar... y usar aquellas energías mayores que me sean beneficiosas y constructivas... "
                "a mí... y a quienes están cerca de mí... También deseo profundamente la ayuda y la cooperación... la asistencia... la comprensión... "
                "de aquellos cuya sabiduría, desarrollo y experiencia sean iguales o mayores que los míos...")

T_RESONANTE_A = ("Ahora, la sintonía resonante... Vamos a hacer cinco respiraciones. Al inhalar, abre los ojos... y siente que entra energía fresca, que llena todo tu cuerpo y sube hasta tu cabeza... "
                 "Retén el aire un momento... Al exhalar, cierra los ojos, y suelta el aire por la boca, con los labios como si fueras a apagar una vela... sintiendo que sale la energía gastada... "
                 "Sigue el círculo de la pantalla: crece al inhalar, se encoge al exhalar...")
T_RESONANTE_B = ("Ahora, a tu propio ritmo, inhala por la nariz... y al exhalar, deja salir el aire con un zumbido... un sonido grave, con las cuerdas vocales... "
                 "como aaah... o uuum... Sube y baja de tono como quieras... Hazlo en voz alta, aunque dé pena. Sigue así hasta que yo te avise... "
                 "Siente cómo vibra el pecho... la garganta... la cabeza...")
T_RESONANTE_FIN = "Respira normal... y relájate... Retén la energía fresca que quedó en tu cabeza... la vas a usar enseguida."

T_REBAL = ("Deja ahora que la energía que guardaste en la cabeza fluya hacia afuera por la coronilla... como una fuente... Gírala hacia abajo, alrededor de ti... por todos lados... "
           "hasta las plantas de los pies... y que vuelva a entrar por los pies... y suba por dentro del cuerpo... y salga otra vez por la cabeza... Un flujo continuo... "
           "Ahora haz que ese flujo gire... en espiral... alrededor de ti, antes de entrar por los pies... Es tu globo de energía resonante... Retiene tu energía... y te protege de cualquier energía que no sea tuya... "
           "Puedes abrir una parte cuando quieras recibir o enviar algo... Mira el orbe de la pantalla: late con tu respiración.")

T_FOCUS3 = ("Voy a contar del uno al tres... Cuando llegue a tres, tu mente y tu cerebro estarán mucho más en unísono... coherentes... completos... Uno... ... Dos... ... Tres... "
            "Relájate... explora... disfruta los patrones de Focus 3... Te llamaré cuando sea hora de volver.")

T_F10_CABEZA = ("Ahora, con los ojos cerrados, deja que la relajación comience en la cabeza... Afloja la mandíbula... deja que los dientes se separen apenas... "
                "Afloja los párpados... pesan... Los labios... la frente... las mejillas... el cuero cabelludo... el cuello... y los músculos de los ojos, que descansan... "
                "Deja que esa relajación se hunda hacia adentro... hacia el cerebro... y tu cerebro le dice a tu cuerpo: relájate... suelta... duerme...")
T_F10_CUENTA = ("Cuatro... los pies y los dedos de los pies... míralos con los ojos cerrados... tu cerebro les dice: relájate, suelta, duerme... "
                "Cinco... las piernas... pantorrillas, rodillas, muslos... relájate, suelta, duerme... "
                "Seis... las caderas... relájate... suelta... duerme... "
                "Siete... el vientre... riñones, hígado, estómago, intestinos... relájate... suelta... duerme... "
                "Ocho... el pecho... el corazón, los pulmones, el diafragma... relájate... suelta... duerme... "
                "Nueve... la espalda... toda la espalda... relájate... suelta... duerme... "
                "Diez... los brazos, las manos, los dedos... los hombros... relájate... suelta... duerme...")
T_F10_DIEZ = ("Diez... ... diez... ... diez... Relájate y permanece tranquilo y cómodo en Focus 10... el estado diez... donde tu mente está brillante y alerta... "
              "y tu cuerpo físico duerme profunda, calmada y cómodamente... Mente despierta... cuerpo dormido...")
T_F10_RAPIDO = ("Voy a contar del uno al diez, y con cada número te hundes más en Focus 10... Uno... dos... tres... cuatro... cinco... seis... siete... ocho... nueve... diez... diez... diez... "
                "Mente despierta, cuerpo dormido.")

T_ANCLAS = ("Mientras estás en Focus 10, aprende estas señales... Cuando quieras estar totalmente despierto y alerta en cualquier momento, toca con los dedos de la mano derecha la parte de atrás de tu cuello, y piensa: uno... "
            "Cuando quieras recordar algo que viviste aquí, cierra los ojos y toca suavemente el centro de tu frente con los dedos de la mano derecha... "
            "Y para entrar a Focus 10 sin este audio, en cualquier lugar seguro: inhala hondo... piensa diez... y exhala... Para volver: piensa uno... y mueve los dedos de la mano derecha.")

T_SALUD = ("Cuando regreses a la vigilia física, tu cuerpo estará tan equilibrado como para superar todo lo que le impida dar lo mejor de sí... "
           "La circulación tan ecualizada como para liberar la tensión de los centros nerviosos... Perfectamente normal... perfectamente equilibrado... perfectamente ecualizado será tu estado al volver...")

T_RETORNO = ("Es hora de volver... Volverás a la plena realidad física despierta mientras cuento del diez al uno... Al llegar al uno, tus cinco sentidos funcionarán clara, limpia, nítida y bellamente... "
             "estarás completamente despierto, física y mentalmente... renovado... mejor en todos los sentidos... Diez... nueve... ocho... siete... el cuerpo empieza a despertar... "
             "seis... cinco... cuatro... sientes las manos, los pies... tres... dos... uno... Despierta. Abre los ojos. Respira hondo. Estira brazos y piernas... Esto completa el ejercicio.")
T_RETORNO_CORTO = ("Es hora de volver... Regresa por el método que ya conoces: piensa el número uno... y mueve los dedos de la mano derecha... Diez... nueve... ocho... siete... seis... cinco... cuatro... tres... dos... uno... "
                   "Despierta. Abre los ojos. Respira hondo. Estira el cuerpo. Esto completa el ejercicio.")

T_PREP_CORTA = ("Prepárate como ya sabes... Primero, la caja de conversión de energía... Segundo, la sintonía resonante... Tercero, el globo de energía resonante... "
                "Cuarto, la afirmación que empieza: soy más que mi cuerpo físico... Y quinto, entra a Focus 10... Te espero ahí.")

# ----------------------------------------------------------------------------- sesiones

ESCALA_PAUSAS = {  # multiplica los silencios tras la voz para acercar cada sesion a la duracion de las cintas originales (30-37 min)
    "f10_intro": 1.4, "f10_avanzado": 1.5, "liberar_recargar": 1.8, "flujo_libre_10": 1.3, "f12_intro": 1.35, "respiracion_color": 1.6,
    "barra_energia": 1.5, "mapa_cuerpo": 1.5, "f15": 1.2, "f21": 1.35,
}

def sesion(id, titulo, onda, orden, descripcion, segmentos, requiere="", dormir=False, nocturna=False):
    return {"id": id, "titulo": titulo, "onda": onda, "orden": orden, "descripcion": descripcion, "requiere": requiere, "dormir": dormir, "nocturna": nocturna, "segmentos": segmentos,
            "escala": ESCALA_PAUSAS.get(id, 1.0)}

ONDA1 = "Onda I - Descubrimiento"
ONDA2 = "Onda II - Umbral"
ONDA3 = "Onda III - Libertad"
LUCIDO = "Sueño lúcido"
CIENCIA = "Laboratorio"

SESIONES = []

# 1 Orientacion ----------------------------------------------------------------------------------
SESIONES.append(sesion("orientacion", "Orientación", ONDA1, 1,
    "La primera sesión del programa. Comprueba los audífonos, demuestra el efecto Hemi-Sync (dos tonos distintos, uno por oído, que el cerebro convierte en un batido), "
    "enseña la caja de conversión de energía, la afirmación, la sintonía resonante y te lleva por primera vez a Focus 3. Unos 14 minutos.",
    [
        seg("Bienvenida", T_CANALES, 3, "oleaje", "cosmos", rampa=4),
        seg("Tono izquierdo", "Oído izquierdo...", 6, "demo_izq", "ganz_azul", rampa=1),
        seg("Tono derecho", "Oído derecho...", 6, "demo_der", "ganz_azul", rampa=1),
        seg("Ambos", "Y ahora los dos a la vez... Escucha: ya no son dos tonos. Hay una onda... un vibrato... un latido lento. Ese latido no existe en el aire: lo crea tu cerebro al comparar los dos oídos. "
            "Tus dos hemisferios empiezan a trabajar juntos, como una unidad. Eso es Hemi-Sync... Quédate un momento explorando esa sensación...", 30, "demo_ambos", "orbe_blanco", rampa=1),
        seg("Acomódate", "Muy bien... Ahora acomoda tu cuerpo. Que no haya puntos de tensión... Si algo aprieta, aflójalo... Escucha el oleaje y deja que el cuerpo se asiente...", 25, "oleaje", "ganz_azul", rampa=6),
        seg("La caja", T_CAJA, 40, "prep", "caja"),
        seg("Cierra la caja", T_CAJA_CIERRA, 10, "prep", "caja"),
        seg("Afirmación", T_AFIRMACION, 15, "prep", "orbe"),
        seg("Sintonía resonante", T_RESONANTE_A, 55, "respira", "orbe", resp=respira(6.0, 0.45)),
        seg("Zumbido", T_RESONANTE_B, 75, "respira", "orbe", resp=respira(5.0, 0.35, False)),
        seg("Retén la energía", T_RESONANTE_FIN, 10, "prep", "orbe"),
        seg("Focus 3", T_FOCUS3, 150, "f3", "mandala_lento", rampa=20),
        seg("Vuelve a visitar", "Sigue en Focus 3... Nota la diferencia con tu estado normal... Observa sin esperar nada...", 90, "f3", "mandala_lento"),
        seg("Retorno", "Es hora de volver... Contaré del tres al uno. Al llegar al uno estarás completamente despierto, alerta y renovado... Tres... dos... uno... Abre los ojos. Respira hondo. Estira el cuerpo. "
            "Esto completa la orientación. En la siguiente sesión aprenderás Focus 10.", 12, "retorno", "cosmos", rampa=6),
    ]))

# 2 Introduccion a Focus 10 ---------------------------------------------------------------------
SESIONES.append(sesion("f10_intro", "Introducción a Focus 10", ONDA1, 2,
    "El corazón de la Onda I: la relajación de diez puntos que lleva a 'mente despierta, cuerpo dormido'. Aprendes las señales (anclas) para entrar y salir sin audio y la afirmación de salud. "
    "Repite esta sesión varios días hasta que Focus 10 te resulte familiar. Unos 35 minutos.",
    [
        seg("Acomódate", "Ponte cómodo... Que no haya puntos de tensión... Escucha el oleaje mientras el cuerpo se asienta...", 25, "oleaje", "cosmos_lento", rampa=4),
        seg("La caja", T_CAJA, 35, "prep", "caja"),
        seg("Cierra la caja", T_CAJA_CIERRA, 8, "prep", "caja"),
        seg("Sintonía resonante", T_RESONANTE_A, 50, "respira", "orbe", resp=respira(6.0, 0.45)),
        seg("Zumbido", T_RESONANTE_B, 70, "respira", "orbe", resp=respira(5.0, 0.35, False)),
        seg("Retén la energía", T_RESONANTE_FIN, 8, "prep", "orbe"),
        seg("Afirmación", T_AFIRMACION, 12, "prep", "orbe"),
        seg("Focus 3", "Uno... dos... tres... Focus 3. Tu mente y tu cerebro, en unísono...", 20, "f3", "mandala_lento", rampa=15),
        seg("Relajación de la cabeza", T_F10_CABEZA, 20, "f10", "ganz_violeta", rampa=25),
        seg("Diez puntos", T_F10_CUENTA, 15, "f10", "ganz_violeta"),
        seg("Focus 10", T_F10_DIEZ, 120, "f10", "orbe", rampa=20),
        seg("Explora", "Explora Focus 10... Puede que pierdas la noción de dónde están tus brazos, tus piernas... puede que sientas un hormigueo, una vibración, calor... o nada en particular. Todo está bien... "
            "Mente despierta... cuerpo dormido...", 180, "f10", "orbe"),
        seg("Señales", T_ANCLAS, 60, "f10", "orbe"),
        seg("Practica", "Practica ahora. Piensa uno y mueve los dedos de la mano derecha: estás de vuelta, alerta... Y ahora inhala hondo, piensa diez... exhala... y vuelves a Focus 10... diez... diez... diez...", 150, "f10", "orbe"),
        seg("Quédate", "Quédate en Focus 10 todo lo que quieras... Yo te llamaré cuando sea hora de volver...", 240, "f10", "orbe_blanco"),
        seg("Afirmación de salud", T_SALUD, 20, "f10", "orbe_dorado"),
        seg("Retorno", T_RETORNO, 15, "retorno", "cosmos", rampa=8),
    ]))

# 3 Focus 10 avanzado ---------------------------------------------------------------------------
SESIONES.append(sesion("f10_avanzado", "Focus 10 avanzado", ONDA1, 3,
    "Introduce el globo de energía resonante (REBAL) y practica entrar y salir de Focus 10 varias veces, cada vez más rápido, hasta que las señales funcionen solas. Unos 33 minutos.",
    [
        seg("Acomódate", "Ponte cómodo, sin puntos de tensión... y respira con el oleaje...", 20, "oleaje", "cosmos_lento", rampa=4),
        seg("La caja", "Primero, tu caja de conversión de energía... Abre la tapa, guarda todo lo que no necesitas, ciérrala y date la vuelta... Tómate tu tiempo.", 40, "prep", "caja"),
        seg("Sintonía resonante", "Ahora la sintonía resonante: cinco respiraciones con los ojos abiertos al inhalar y cerrados al exhalar... y después el zumbido, a tu ritmo, hasta que te avise...", 110, "respira", "orbe", resp=respira(5.5, 0.4)),
        seg("Retén la energía", T_RESONANTE_FIN, 6, "prep", "orbe"),
        seg("REBAL", T_REBAL, 60, "prep", "orbe", resp=respira(6.0, 0.45, False)),
        seg("Atajo del REBAL", "Aprende el atajo para formar tu globo en cualquier momento: inhala hondo... y reteniendo el aire, imagina un círculo brillante, en movimiento, con el número diez dentro... "
            "Al exhalar, sopla ese círculo hacia afuera, alrededor de ti... Ya está... Al terminar el ejercicio se reabsorbe solo.", 25, "prep", "orbe"),
        seg("Afirmación", T_AFIRMACION, 10, "prep", "orbe"),
        seg("A Focus 10", T_F10_RAPIDO, 90, "f10", "ganz_violeta", rampa=20),
        seg("Explora", "Explora Focus 10 a tu manera... Observa... Mente despierta... cuerpo dormido...", 150, "f10", "orbe"),
        seg("Sal", "Ahora regresa: piensa uno... y mueve los dedos de la mano derecha... Estás despierto, alerta... Siente el cuerpo...", 20, "retorno", "ganz_azul", rampa=5),
        seg("Vuelve", "Y entra otra vez: inhala hondo... piensa diez... exhala... diez... diez... diez... Focus 10...", 120, "f10", "orbe", rampa=10),
        seg("Sal otra vez", "Uno... mueve los dedos... despierto, alerta...", 15, "retorno", "ganz_azul", rampa=4),
        seg("Última vez", "Diez... Focus 10... cada vez más rápido, cada vez más profundo... Quédate aquí todo lo que quieras... te llamaré para volver.", 300, "f10", "orbe_blanco", rampa=8),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado"),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ]))

# 4 Liberar y recargar --------------------------------------------------------------------------
SESIONES.append(sesion("liberar_recargar", "Liberar y recargar", ONDA1, 4,
    "Ejercicio diario: en Focus 10 sacas de la caja un miedo o una emoción, la observas, le quitas la carga y recuperas el recuerdo limpio. Después recargas energía. "
    "Según Monroe, es la sesión que más conviene repetir. Unos 30 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 180, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 10", T_F10_RAPIDO, 60, "f10", "orbe", rampa=20),
        seg("Abre la caja", "Ahora, desde Focus 10, mira tu caja de conversión de energía... Ábrela... y saca de ella un solo miedo... o una emoción que te pese... Solo uno... Míralo desde aquí, con calma... "
            "No eres tú: es algo que llevas puesto... Observa cuándo apareció... para qué te sirvió...", 90, "f10", "caja"),
        seg("Libera", "Ahora, al exhalar, suelta la carga que lleva... como si abrieras la mano... Lo que queda es el recuerdo, limpio, sin peso... Guárdalo donde quieras, pero fuera de la caja... "
            "Repite con otro, si quieres... yo espero...", 150, "f10", "tunel_lento", resp=respira(6.0, 0.4, False)),
        seg("Recarga", "Ahora recarga... Deja que entre energía fresca por la coronilla, con cada inhalación... y que llene cada parte del cuerpo... brillante... cálida... hasta las puntas de los dedos... "
            "Exhala solo lo gastado...", 120, "f10", "orbe_dorado", resp=respira(6.0, 0.45)),
        seg("Descansa", "Descansa en Focus 10, lleno de energía nueva...", 180, "f10", "orbe_blanco"),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado"),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ]))

# 5 Exploracion del sueno -----------------------------------------------------------------------
SESIONES.append(sesion("exploracion_sueno", "Exploración del sueño", ONDA1, 5,
    "Para la noche: desde Focus 10 aprendes a 'salir rodando' del cuerpo, a flotar y a volver, y después el conteo del once al veinte te lleva a dormir de forma natural. "
    "El audio sigue el procesador de sueño de la patente de Monroe (theta, delta, delta profundo) y se desvanece sin despertarte. Déjalo correr: no hay retorno.",
    [
        seg("Prepárate", "Esta sesión termina dormido. Apaga lo que tengas que apagar... Prepárate como ya sabes: la caja... la sintonía resonante, suave, sin despertar a nadie... el globo de energía... la afirmación... y Focus 10.", 150, "prep", "ganz_oscuro", rampa=6),
        seg("Focus 10", T_F10_RAPIDO, 60, "f10_sueno", "orbe", rampa=25),
        seg("Rueda", "Ahora, en Focus 10, imagina que tu cuerpo es un tronco... y que puedes rodar... Rueda hacia un lado, suavemente... sin mover un músculo... y vuelve... Rueda al otro lado... y vuelve... "
            "Siente lo ligero que es el que rueda...", 90, "f10_sueno", "orbe"),
        seg("Flota", "Ahora flota hacia arriba... despacio... unos centímetros... Mira el techo cerca de ti... y baja... y vuelve a tu cuerpo, que sigue dormido y seguro... Hazlo otra vez, a tu ritmo...", 120, "f10_sueno", "vacio"),
        seg("Hacia el sueño", "Muy bien... Ahora voy a contar del once al veinte, y con cada número te acercas al sueño natural, profundo y reparador... Mañana recordarás lo que quieras recordar... "
            "Once... doce... trece... más profundo... catorce... quince... dieciséis... diecisiete... dieciocho... diecinueve... veinte... Duerme...", 240, "sueno_theta", "vacio", rampa=30, voz_gain=0.5),
        seg("Theta", None, duracion=360, sonido="sueno_theta", visual="vacio", rampa=60),
        seg("Delta", None, duracion=600, sonido="sueno_delta", visual="negro", rampa=90),
        seg("Delta profundo", None, duracion=600, sonido="sueno_profundo", visual="negro", rampa=120),
        seg("Silencio", None, duracion=120, sonido="nada", visual="negro", rampa=120),
    ], dormir=True))

# 6 Flujo libre 10 ------------------------------------------------------------------------------
SESIONES.append(sesion("flujo_libre_10", "Flujo libre 10", ONDA1, 6,
    "Mínima guía: entras a Focus 10 con un propósito propio y exploras en silencio largo, con las señales Hemi-Sync de fondo. Cierra la Onda I. Unos 35 minutos.",
    [
        seg("Propósito", "Antes de empezar, decide un propósito para esta sesión... una pregunta... algo que quieras explorar... o simplemente estar... Tenlo claro... y prepárate como ya sabes.", 170, "prep", "ganz_azul", resp=respira(6.0, 0.42, False)),
        seg("Focus 10", T_F10_RAPIDO, 30, "f10", "orbe", rampa=20),
        seg("Flujo libre", "Focus 10... Flujo libre... Es tuyo...", 600, "f10", "tunel_lento", rampa=40),
        seg("Sigue", None, duracion=600, sonido="f10", visual="mandala_lento", rampa=60),
        seg("Recuerda", "Si hay algo que quieras recordar, tócate el centro de la frente... y guárdalo...", 180, "f10", "orbe"),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f10_intro"))

# 7 Introduccion a Focus 12 ---------------------------------------------------------------------
SESIONES.append(sesion("f12_intro", "Introducción a Focus 12", ONDA2, 7,
    "Onda II. Desde Focus 10 se añaden las señales de Focus 12 (alfa a 10 Hz sobre 400-500 Hz): conciencia expandida. El domo pasa del orbe al túnel y al mandala. "
    "Opcionalmente, un flicker suave de 8 Hz con los ojos cerrados durante la exploración. Unos 38 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 170, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 10", T_F10_RAPIDO, 60, "f10", "orbe", rampa=20),
        seg("Hacia Focus 12", "Ahora simplemente relájate mientras te guío de Focus 10 a Focus 12... y observa las diferencias... Diez... once... doce... Focus 12... conciencia expandida... "
            "La energía es más alta, el cuerpo sigue profundamente dormido... Puede que veas colores, símbolos... que sepas cosas sin palabras... Observa...", 180, "f12", "tunel", rampa=40),
        seg("Expande", "Deja que tu conciencia se expanda en todas direcciones... más allá de la habitación... más allá de la casa... sin límite...", 240, "f12", "mandala", rampa=40),
        seg("Luz", "Si lo activaste, ahora una luz suave latirá sobre tus párpados cerrados. Déjala entrar... y mira lo que aparece...", 300, "f12", "mandala", flick=flicker(8.0, 0.35), rampa=20),
        seg("Explora", "Explora Focus 12... Es tuyo...", 420, "f12", "tunel_lento", rampa=30),
        seg("Vuelve a 10", "Ahora regresa a Focus 10... doce... once... diez... y nota el cambio...", 60, "f10", "orbe", rampa=25),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado"),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f10_avanzado"))

# 8 Respiracion de color ------------------------------------------------------------------------
SESIONES.append(sesion("respiracion_color", "Respiración de color", ONDA2, 8,
    "En Focus 12 respiras colores con un propósito: verde para calmar, rojo para fuerza, púrpura para restaurar. El domo se tiñe del color que respiras (Ganzfeld) y la guía de respiración va a 6 por minuto, "
    "la frecuencia de resonancia cardiaca con mejor evidencia. Termina viendo el cuerpo entero y perfecto. Unos 30 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 150, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 12", "Diez... Focus 10... y doce... Focus 12... conciencia expandida...", 60, "f12", "tunel", rampa=30),
        seg("Verde", "Ahora respira verde... Con cada inhalación entra un verde fresco, claro... que calma... que reduce la energía emocional sobrante... Llévalo a donde haga falta... y exhala lo que sobra...", 150, "f12", "ganz_verde", resp=respira(6.0, 0.42), rampa=20),
        seg("Rojo", "Ahora respira rojo... un rojo vivo, brillante... fuerza... velocidad... vitalidad... Llévalo a los músculos, a la sangre... y exhala el cansancio...", 150, "f12", "ganz_rojo", resp=respira(6.0, 0.42), rampa=20),
        seg("Púrpura", "Ahora respira púrpura... profundo, luminoso... el color que restaura... que repara... Llévalo a cualquier parte del cuerpo que lo necesite... y déjalo ahí...", 180, "f12", "ganz_purpura", resp=respira(6.0, 0.42), rampa=20),
        seg("Entero y perfecto", "Ahora mira tu cuerpo entero... completo... perfecto... brillando con los tres colores en equilibrio... Guarda esa imagen...", 120, "f12", "orbe_blanco", rampa=20),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado", rampa=20),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f12_intro"))

# 9 Barra de energia ----------------------------------------------------------------------------
SESIONES.append(sesion("barra_energia", "Barra de energía", ONDA2, 9,
    "La herramienta de energía: un punto de luz pulsante que se estira en una barra, luego en un cilindro, y que puedes dirigir. Visualmente, orbe → túnel → vórtice. Unos 30 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 150, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 12", "Diez... Focus 10... doce... Focus 12...", 50, "f12", "tunel", rampa=30),
        seg("El punto", "Frente a ti, en Focus 12, aparece un punto de luz... pequeño, brillante, que late... Mira cómo late... Puedes hacerlo más brillante... más intenso... Es tu energía...", 90, "f12", "orbe_blanco", rampa=20),
        seg("La barra", "Ahora estira ese punto... hacia arriba y hacia abajo... hasta formar una barra de luz, un cilindro vertical, que late con la misma energía... Puedes hacerlo tan grande como quieras...", 90, "f12", "orbe_blanco"),
        seg("Dirige", "Ahora la barra se inclina... y puedes apuntarla... a una parte de tu cuerpo que necesite energía... o hacia una persona... o hacia un problema... Deja que la energía fluya por ella... y observa lo que pasa...", 240, "f12", "tunel", rampa=20),
        seg("El vórtice", "La barra gira... más y más rápido... hasta convertirse en un vórtice... un túnel de energía que puede llevarte a donde quieras... Entra... y explora...", 300, "f12", "tunel_fuego", rampa=25),
        seg("Recoge", "Ahora recoge la barra... vuelve a ser un punto de luz... y guárdalo dentro de ti, listo para la próxima vez...", 60, "f12", "orbe_dorado", rampa=20),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado", rampa=20),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f12_intro"))

# 10 Mapa del cuerpo vivo ----------------------------------------------------------------------
SESIONES.append(sesion("mapa_cuerpo", "Mapa del cuerpo vivo", ONDA2, 10,
    "Recorrido completo del cuerpo en Focus 12: contorno blanco, circulación roja, nervios azules, glándulas amarillas, músculo y hueso naranja; después, luz púrpura donde haga falta. "
    "Es también una rotación de atención corporal al estilo del yoga nidra (NSDR), con evidencia de reducción de estrés. Unos 32 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 150, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 12", "Diez... Focus 10... doce... Focus 12...", 50, "f12", "tunel", rampa=30),
        seg("Contorno", "Frente a ti aparece el contorno de tu cuerpo, dibujado en luz blanca... de pie... completo... Míralo...", 45, "f12", "ganz_oscuro", rampa=20),
        seg("Rojo", "Ahora, dentro del contorno, se enciende en rojo la circulación... el corazón... las arterias... las venas... hasta los capilares más pequeños... Observa dónde fluye bien... y dónde no...", 100, "f12", "ganz_rojo", rampa=15),
        seg("Azul", "Ahora en azul, el sistema nervioso... el cerebro... la médula... los nervios que llegan a cada dedo... Observa...", 100, "f12", "ganz_azul", rampa=15),
        seg("Amarillo", "En amarillo, las glándulas... la pineal... la hipófisis... la tiroides... las suprarrenales... Observa...", 90, "f12", "ganz_naranja", rampa=15),
        seg("Naranja", "Y en naranja, los músculos y los huesos... todo el soporte del cuerpo... Observa...", 90, "f12", "ganz_naranja", rampa=15),
        seg("Recorrido", "Ahora recorre tu cuerpo real, punto por punto, dejando que cada parte se afloje al nombrarla... Pulgar derecho... índice... medio... anular... meñique... la palma... la muñeca... el codo... el hombro... "
            "Pulgar izquierdo... índice... medio... anular... meñique... la palma... la muñeca... el codo... el hombro... La frente... los ojos... la nariz... la boca... la garganta... el pecho... el abdomen... la cadera... "
            "La pierna derecha, hasta los dedos del pie... la pierna izquierda, hasta los dedos del pie... Todo el cuerpo, a la vez...", 120, "f12", "ganz_violeta", rampa=20),
        seg("Púrpura", "Ahora toma tu barra de energía, púrpura... y llévala a cualquier lugar del mapa que lo necesite... Déjala ahí el tiempo necesario...", 240, "f12", "ganz_purpura", rampa=20),
        seg("Entero y perfecto", "Mira el mapa completo... entero... perfecto... y guárdalo... Es tu cuerpo, y sabes cómo cuidarlo...", 60, "f12", "orbe_blanco", rampa=20),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado", rampa=20),
        seg("Retorno", T_RETORNO_CORTO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f12_intro"))

# 11 Luz: Ganzflicker ---------------------------------------------------------------------------
SESIONES.append(sesion("ganzflicker", "Luz: Ganzflicker", CIENCIA, 11,
    "Sesión científica (Bartossek 2021, Amaya 2023, Reeder 2022): luz intermitente a 10 Hz sobre los párpados cerrados durante 15 minutos, con batido alfa de 10 Hz y ruido rosa. "
    "El 70-90 % de las personas ve geometrías, colores y movimiento; muchas, escenas. Requiere el consentimiento de flicker y no debe usarse con epilepsia o migraña con aura. Unos 25 minutos.",
    [
        seg("Oscuridad", "Esta sesión usa luz intermitente. Cierra los ojos y mantenlos cerrados todo el tiempo... la luz atraviesa los párpados... Si en algún momento te sientes mal, pulsa Escape y la luz se apaga... "
            "Primero, dos minutos de oscuridad y respiración lenta...", 100, "respira", "negro", resp=respira(6.0, 0.42, False), rampa=6),
        seg("Rampa", "La luz empieza, muy suave, y va subiendo durante un minuto... No busques nada: deja que aparezca lo que aparezca...", 55, "alfa10", "ganz_oscuro", flick=flicker(10.0, 0.25, 1, (1.0, 0.9, 0.75)), rampa=60),
        seg("Flicker 10 Hz", None, duracion=300, sonido="alfa10", visual="ganz_oscuro", flick=flicker(10.0, 0.6, 1, (1.0, 0.9, 0.75)), rampa=60),
        seg("Observa", "Formas... colores... movimiento... quizá escenas... Obsérvalas como quien mira el fuego...", 300, "alfa10", "ganz_oscuro", flick=flicker(10.0, 0.7, 1, (1.0, 0.92, 0.8)), rampa=30),
        seg("Más", None, duracion=300, sonido="alfa10", visual="ganz_oscuro", flick=flicker(10.0, 0.7, 1, (1.0, 0.92, 0.8)), rampa=30),
        seg("Fundido", "La luz se va apagando despacio... Quédate con lo que viste...", 110, "theta6", "negro", flick=FLICKER_OFF, rampa=90),
        seg("Reporte", "Antes de abrir los ojos, repasa lo que apareció: formas... colores... escenas... emociones... Cuando quieras, abre los ojos despacio... y anótalo.", 40, "retorno", "cosmos_lento", rampa=15),
    ]))

# 12 Focus 15 -----------------------------------------------------------------------------------
SESIONES.append(sesion("f15", "Focus 15: no-tiempo", ONDA3, 12,
    "Onda III. De Focus 12 a Focus 15, el estado sin tiempo: señales theta de 7 Hz sobre 500-750 Hz, silencio largo y un domo casi vacío. Monroe advierte que menos del 5 % lo alcanza en el primer intento: "
    "vuelve a esta sesión cuantas veces quieras. Unos 42 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 150, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 12", "Diez... Focus 10... doce... Focus 12... conciencia expandida...", 90, "f12", "tunel", rampa=30),
        seg("Hacia Focus 15", "Ahora vamos más allá de Focus 12... hacia Focus 15... el estado de no-tiempo... Trece... el cuerpo queda muy lejos... catorce... las señales del cuerpo se apagan... quince... Focus 15... "
            "Aquí el tiempo lineal ya no importa... Tres horas pueden parecer dos minutos...", 180, "f15", "vacio", rampa=60),
        seg("No-tiempo", "Focus 15... No hay prisa, porque no hay tiempo...", 600, "f15", "vacio", rampa=60),
        seg("El pasado", "Si quieres, desde aquí puedes mirar hacia atrás... el tiempo es una rueda, y tú estás en el centro... elige un radio... y mira...", 420, "f15", "cosmos_lento", rampa=60),
        seg("Silencio", None, duracion=420, sonido="f15", visual="vacio", rampa=60),
        seg("Regresa a 12", "Es hora de regresar... quince... catorce... trece... doce... Focus 12... diez... Focus 10...", 60, "f10", "orbe", rampa=40),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado"),
        seg("Retorno", T_RETORNO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f12_intro"))

# 13 Focus 21 -----------------------------------------------------------------------------------
SESIONES.append(sesion("f21", "Focus 21: el puente", ONDA3, 13,
    "El borde del tiempo-espacio: señales de Focus 21 (beta de 16 Hz sobre 600-900 Hz, con theta de 4 Hz debajo). Se transita por los estados intermedios y se pide guía a 'quienes tienen sabiduría igual o mayor'. "
    "El domo se abre a la galaxia. Unos 45 minutos.",
    [
        seg("Prepárate", T_PREP_CORTA, 150, "prep", "orbe_dorado", resp=respira(6.0, 0.42, False)),
        seg("Focus 12", "Diez... Focus 10... doce... Focus 12...", 60, "f12", "tunel", rampa=30),
        seg("Hacia 21", "Ahora, desde Focus 12, vamos a transitar por los estados intermedios hasta Focus 21... el puente... Quince... no-tiempo... dieciocho... amor y aceptación sin condiciones... "
            "veintiuno... Focus 21... el borde mismo de la percepción del tiempo y el espacio... el puente hacia otros sistemas de energía...", 180, "f21", "tunel", rampa=70),
        seg("El puente", "Focus 21... Pide la guía y la protección de aquellos cuya sabiduría es igual o mayor que la tuya... y espera... sin expectativas...", 480, "f21", "galaxia", rampa=60),
        seg("Luz", "Si lo activaste, una luz suave te acompaña...", 420, "f21", "anillo", flick=flicker(7.83, 0.3, 0, (0.85, 0.9, 1.0)), rampa=40),
        seg("Silencio", None, duracion=480, sonido="f21", visual="galaxia", rampa=60),
        seg("Regresa", "Es hora de regresar por el puente... veintiuno... dieciocho... quince... doce... Focus 12... diez... Focus 10... Tócate la frente para recordar lo que quieras recordar...", 60, "f10", "orbe", rampa=50),
        seg("Afirmación de salud", T_SALUD, 15, "f10", "orbe_dorado"),
        seg("Retorno", T_RETORNO, 15, "retorno", "cosmos", rampa=8),
    ], requiere="f15"))

# 14 Sueno lucido: entrenamiento --------------------------------------------------------------------
T_LUCIDO_GUIA = ("Al notar la señal, te vuelves lúcido... Lleva la atención a tus pensamientos... observa tu cuerpo... tu respiración... Pregúntate: ¿estoy soñando?... "
                 "Busca algo incongruente... algo que no cuadre... Nota en qué se diferencia esta experiencia de la vigilia normal...")

def entrenamiento_lucido():
    segs = [
        seg("Qué vas a hacer", "Esta sesión entrena una señal para reconocer que sueñas: la técnica de reactivación de lucidez de Northwestern. Primero asociarás una campana a un estado lúcido... "
            "Después, mientras te duermes, la campana sonará con intervalos cada vez más largos... y esta noche, con la sesión nocturna, volverá a sonar suavemente durante tus sueños. "
            "Si la escuchas dentro de un sueño, sabrás que estás soñando.", 8, "prep", "ganz_oscuro", rampa=6),
    ]
    for i in range(4):
        segs.append(seg("Señal %d" % (i + 1), T_LUCIDO_GUIA, 45, "theta6", "orbe", campana=True, campana_hz=528.0, campana_gain=0.4, rampa=5))
    segs.append(seg("A dormir", "Muy bien... Ahora acomódate para dormir. La señal seguirá sonando con pausas cada vez más largas... Cada vez que la oigas, piensa: estoy soñando... y déjate ir.", 10, "theta6", "vacio", rampa=10, voz_gain=0.5))
    intervalos = [45, 70, 55, 65, 70, 80, 65, 60, 75, 75, 90, 120]
    for i, iv in enumerate(intervalos):
        segs.append(seg("Señal mientras duermes %d" % (i + 1), None, duracion=iv, sonido="sueno_theta", visual="vacio", campana=True, campana_hz=528.0, campana_gain=0.28 - i * 0.008, rampa=20))
    segs.append(seg("Delta", None, duracion=600, sonido="sueno_delta", visual="negro", rampa=90))
    segs.append(seg("Silencio", None, duracion=120, sonido="nada", visual="negro", rampa=90))
    return segs

SESIONES.append(sesion("lucido_entrenamiento", "Sueño lúcido: entrenamiento", LUCIDO, 14,
    "Protocolo TLR (Konkoly, Paller et al. 2021-2024): cuatro señales con guía verbal, luego doce señales mientras te duermes, con intervalos de 45 a 120 segundos. "
    "Úsala antes de dormir y después lanza 'Sueño lúcido: noche'. En el estudio, los sueños lúcidos por semana pasaron de 0.7 a 2.1. Unos 35 minutos; termina dormido.",
    entrenamiento_lucido(), dormir=True))

# 15 Sueno lucido: noche ----------------------------------------------------------------------------
def noche_lucida():
    segs = [seg("Noche", "Buenas noches... La pantalla se apagará. Dentro de seis horas la señal empezará a sonar, muy suave, durante tus sueños... Si la oyes en un sueño: estás soñando.", 5, "nada", "negro", rampa=5, voz_gain=0.5)]
    segs.append(seg("Dormir", None, duracion=6 * 3600 - 60, sonido="nada", visual="negro", rampa=30))
    # 2 horas de cues cada 30 s, volumen desde muy bajo subiendo 0.16 % por cue (Konkoly 2024), con una voz susurrada cada 10 cues
    n = int(2 * 3600 / 30)
    for i in range(n):
        g = min(0.3, 0.05 * (1.0016 ** i) + i * 0.0006)
        if i % 10 == 9:
            segs.append(seg("Cue %d" % (i + 1), "Estás soñando...", 27, "nada", "negro", campana=True, campana_hz=528.0, campana_gain=g, rampa=2, voz_gain=0.25))
        else:
            segs.append(seg("Cue %d" % (i + 1), None, duracion=30, sonido="nada", visual="negro", campana=True, campana_hz=528.0, campana_gain=g, rampa=2))
    segs.append(seg("Fin", None, duracion=60, sonido="nada", visual="negro", rampa=5))
    return segs

SESIONES.append(sesion("lucido_noche", "Sueño lúcido: noche", LUCIDO, 15,
    "Protocolo nocturno TLR: seis horas de silencio y pantalla negra; después, durante dos horas (la fase rica en REM), la misma campana del entrenamiento cada 30 segundos, subiendo de volumen muy despacio, "
    "con un susurro cada cinco minutos. Deja los audífonos puestos y la computadora encendida. Pulsa Escape si te despierta. Dura 8 horas.",
    noche_lucida(), requiere="lucido_entrenamiento", nocturna=True))

# 16 Incubacion de suenos (Dormio Light) ------------------------------------------------------------
def incubacion():
    segs = [
        seg("Elige un tema", "Esta sesión incuba un tema en tus sueños, siguiendo el protocolo Dormio del MIT. Elige ahora una idea, un problema o una imagen en la que quieras soñar... Dilo en voz baja... y guárdalo.", 20, "prep", "ganz_oscuro", rampa=6),
        seg("Yoga nidra", "Acuéstate... Recorre el cuerpo mientras lo nombro, soltando cada parte... Mano derecha... brazo derecho... hombro... Mano izquierda... brazo... hombro... Pie derecho... pierna derecha... Pie izquierdo... pierna izquierda... "
            "La espalda... el vientre... el pecho... la garganta... la cara... la cabeza... Todo el cuerpo, pesado y tranquilo... Respira... y cuenta hacia atrás cada exhalación, desde veinte...", 300, "theta6", "vacio", resp=respira(5.0, 0.4, False), rampa=20),
    ]
    for ciclo in range(4):
        segs.append(seg("Ventana %d" % (ciclo + 1), None, duracion=420, sonido="sueno_theta", visual="negro", rampa=40))
        segs.append(seg("Incuba %d" % (ciclo + 1), "Recuerda... piensa en lo que elegiste... déjalo entrar en tus imágenes...", 180, "sueno_theta", "negro", voz_gain=0.35, rampa=5))
        segs.append(seg("Despierta %d" % (ciclo + 1), "Cuéntame qué pasa por tu mente... en voz baja... todo lo que aparezca...", 60, "nada", "ganz_oscuro", campana=True, campana_gain=0.2, voz_gain=0.5, rampa=5))
        segs.append(seg("Recupera %d" % (ciclo + 1), "Gracias... vuelve a dejarte ir...", 420, "sueno_theta", "negro", voz_gain=0.35, rampa=20))
    segs.append(seg("Duerme", "Ahora duerme de verdad... Mañana anota lo que recuerdes.", 60, "sueno_delta", "negro", voz_gain=0.35, rampa=30))
    segs.append(seg("Delta", None, duracion=600, sonido="sueno_delta", visual="negro", rampa=120))
    segs.append(seg("Silencio", None, duracion=120, sonido="nada", visual="negro", rampa=120))
    return segs

SESIONES.append(sesion("incubacion", "Incubación de sueños", LUCIDO, 16,
    "Dormio Light (MIT, 2024): tras un yoga nidra breve, cuatro ciclos de ventana de sueño ligero → frase de incubación → despertar suave con una pregunta → recuperación. "
    "El 91 % de los participantes incorporó el tema en sus imágenes hipnagógicas. Di tus reportes en voz alta o grábalos. Unos 85 minutos; termina dormido.",
    incubacion(), dormir=True))

# 17 Senales Hemi-Sync 1973 (audio oficial del Monroe Institute) ------------------------------------
def hemisync1973():
    dur = 2067.69 / 10.0
    visuales = ["cosmos_lento", "ganz_azul", "orbe", "tunel_lento", "mandala_lento", "orbe_blanco", "tunel", "mandala", "galaxia", "cosmos"]
    segs = [seg("Aviso", "Vas a escuchar las señales Hemi-Sync originales de 1973, publicadas por el Monroe Institute, sin voz. Prepárate como ya sabes y déjate llevar... "
                "Si no suena nada, ejecuta Tools/descargar_hemisync1973.py.", 4, "nada", "cosmos_lento", rampa=6)]
    for i in range(10):
        s = seg("Hemi-Sync 1973, parte %d" % (i + 1), None, duracion=round(dur, 2), sonido="nada", visual=visuales[i], rampa=40)
        s["archivo"] = "../Audio/hemisync1973/%02d.wav" % (i + 1)
        segs.append(s)
    segs.append(seg("Retorno", T_RETORNO_CORTO, 10, "retorno", "cosmos", rampa=8))
    return segs

SESIONES.append(sesion("hemisync1973", "Señales Hemi-Sync 1973 (audio oficial)", CIENCIA, 17,
    "El único audio Hemi-Sync que el Monroe Institute publica gratis (CC BY-NC-ND 4.0): 34 minutos de señales puras de 1973, sin voz, reproducidas tal cual con los visuales 3D del domo. "
    "Requiere descargarlo una vez con Tools/descargar_hemisync1973.py (356 MB, no está en el repositorio).",
    hemisync1973(), requiere="f10_intro"))

# ----------------------------------------------------------------------------- TTS y JSON

def tts(texto, ruta):
    """Sintetiza `texto` a `ruta` (WAV). Devuelve la duracion en segundos."""
    import soundfile as sf
    if not os.path.exists(ruta):
        if DRY:
            return max(1.5, len(texto) * 0.075)
        from openai import OpenAI
        client = OpenAI()
        r = client.audio.speech.create(model=TTS_MODEL, voice=TTS_VOICE, input=texto, response_format="wav", instructions=TTS_INSTRUCTIONS)
        os.makedirs(os.path.dirname(ruta), exist_ok=True)
        bruto = ruta + ".raw.wav"
        with open(bruto, "wb") as f:
            f.write(r.content)
        import subprocess
        subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-i", bruto, "-af", "atempo=%.3f" % VOZ_TEMPO, "-ar", "24000", "-sample_fmt", "s16", ruta])
        os.remove(bruto)
        print("   tts  %s (%d chars)" % (os.path.basename(ruta), len(texto)))
    d, sr = sf.read(ruta)
    return len(d) / float(sr)


def compilar(s):
    sid = s["id"]
    print("== %s (%s)" % (s["titulo"], sid))
    segs_out = []
    t = 0.0
    for i, g in enumerate(s["segmentos"]):
        voz = None
        dur_voz = 0.0
        if g["texto"]:
            h = hashlib.sha1((TTS_VOICE + "|" + TTS_INSTRUCTIONS + "|%.3f|" % VOZ_TEMPO + g["texto"]).encode("utf-8")).hexdigest()[:8]
            rel = "%s/%02d_%s.wav" % (sid, i + 1, h)
            dur_voz = tts(g["texto"], os.path.join(VOICE_DIR, rel))
            voz = rel
        dur = g["duracion"] if g["duracion"] is not None else dur_voz + g["pausa"] * s["escala"]
        o = {"nombre": g["nombre"], "inicio": round(t, 2), "duracion": round(dur, 2), "rampa": g["rampa"]}
        if voz:
            o["voz"] = voz
            o["voz_gain"] = g["voz_gain"]
        if g.get("archivo"):
            o["voz"] = g["archivo"]
            o["voz_gain"] = 0.8
        if g["pantalla"]:
            o["texto"] = g["pantalla"]
        o["sonido"] = SONIDO[g["sonido"]]
        o["visual"] = VISUAL[g["visual"]]
        o["flicker"] = g["flicker"] if g["flicker"] is not None else FLICKER_OFF
        if g["resp"]:
            o["respiracion"] = g["resp"]
        if g["campana"]:
            o["campana"] = True
            o["campana_hz"] = g["campana_hz"]
            o["campana_gain"] = round(g["campana_gain"], 4)
        segs_out.append(o)
        t += dur
    out = {"id": sid, "titulo": s["titulo"], "onda": s["onda"], "orden": s["orden"], "descripcion": s["descripcion"], "requiere": s["requiere"],
           "dormir": s["dormir"], "nocturna": s["nocturna"], "duracion_total": round(t, 2), "segmentos": segs_out}
    os.makedirs(SESS_DIR, exist_ok=True)
    ruta = os.path.join(SESS_DIR, "%02d_%s.json" % (s["orden"], sid))
    with open(ruta, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print("   -> %s  (%d:%02d, %d segmentos)" % (os.path.basename(ruta), int(t // 60), int(t % 60), len(segs_out)))


if __name__ == "__main__":
    for s in SESIONES:
        if SOLO and s["id"] != SOLO:
            continue
        compilar(s)
