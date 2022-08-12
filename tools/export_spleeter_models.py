# This script will be called in generate_release_models.bat

# Modified from https://github.com/gvne/spleeterpp/blob/master/cmake/export_spleeter_models.py

import os
import json
import argparse
import tempfile
import shutil

import tensorflow as tf

import spleeter
from spleeter.model import model_fn

from pprint import pprint


SPLEETER_ROOT = os.path.dirname(spleeter.__file__)


def export_model(pretrained_models_dir: str, frequency_bin_count: int, model_name: str, export_directory: str):
    # read the json parameters
    param_path = os.path.join(SPLEETER_ROOT, "resources", model_name + ".json")
    with open(param_path) as parameter_file:
        parameters = json.load(parameter_file)
    parameters['F'] = frequency_bin_count
    # parameters['mask_extension'] = 'average'
    parameters['MWF'] = False  # default parameter
    parameters['stft_backend'] = 'tensorflow'

    # pprint(model_fn)
    # quit()

    # create the estimator
    configuration = tf.estimator.RunConfig(session_config=tf.compat.v1.ConfigProto())
    estimator = tf.estimator.Estimator(
        model_fn=model_fn,
        model_dir=os.path.join(pretrained_models_dir, model_name),
        params=parameters,
        config=configuration
    )

    # convert it to predictor
    def receiver():
        shape = (None, parameters['n_channels'])
        features = {
            'waveform': tf.compat.v1.placeholder(tf.float32, shape=shape, name='input_waveform'),
            'audio_id': tf.compat.v1.placeholder(tf.string)
        }
        return tf.estimator.export.ServingInputReceiver(features, features)
    # export the estimator into a temp directory
    estimator.export_saved_model(export_directory, receiver)


def main():
    parser = argparse.ArgumentParser(description='Export spleeter models')
    parser.add_argument("pretrained_models_dir")
    parser.add_argument("exported_models_dir")
    parser.add_argument("frequency_limit")
    args = parser.parse_args()

    print("SPLEETER_ROOT           = " + SPLEETER_ROOT)
    print()

    if args.frequency_limit.lower() == '11khz':
        model_dir_suffix = ''
        frequency_bin_count = 1024
    elif args.frequency_limit.lower() == '16khz':
        model_dir_suffix = '-16khz'
        frequency_bin_count = 1536
    elif args.frequency_limit.lower() == '22khz':
        model_dir_suffix = '-22khz'
        frequency_bin_count = 2048
    else:
        print('Wrong frequency_limit argument value: ' + args.frequency_limit.lower())
        sys.exit(-1)    // TODO
        return

    os.makedirs(args.exported_models_dir, exist_ok=True)

    for model in os.listdir(args.pretrained_models_dir):
        # the model is exported under a timestamp. We export in a temp dir,
        # then we move the created folder to the right export path
        destination = os.path.join(args.exported_models_dir, model + model_dir_suffix)
        temp_dir = tempfile.mkdtemp()
        print("pretrained_models_dir   = " + args.pretrained_models_dir)
        print("frequency_bin_count     = " + str(frequency_bin_count))
        print("export_model_name       = " + model + model_dir_suffix)
        print("export_model_dir        = " + destination)
        print("temp_dir                = " + temp_dir)
        print()
        export_model(args.pretrained_models_dir, frequency_bin_count, model, temp_dir)
        created_dir = os.path.join(temp_dir, os.listdir(temp_dir)[0])
        shutil.move(created_dir, destination)
        shutil.rmtree(temp_dir)  # cleanup


if __name__ == '__main__':
    main()
